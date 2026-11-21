"""Build, install, and optionally smoke-test the three WASI CLI tools."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def make_cross_file(source, build, sdk):
    suffix = ".exe" if os.name == "nt" else ""
    sdk_bin = sdk / "bin"
    clang = sdk_bin / ("clang" + suffix)
    ar = sdk_bin / ("llvm-ar" + suffix)
    strip = sdk_bin / ("llvm-strip" + suffix)
    sysroot = sdk / "share" / "wasi-sysroot"
    for path in (clang, ar, strip, sysroot):
        if not path.exists():
            sys.exit(f"WASI SDK path is missing {path}")

    compiler = [
        Path(sys.executable).as_posix(),
        (source / "build-aux/wasi-clang.py").as_posix(),
        clang.as_posix(),
        "--target=wasm32-wasip1",
        f"--sysroot={sysroot.as_posix()}",
    ]
    # Keep machine-file paths slash-separated for both Meson and Windows.
    cross_file = build / "wasi-sdk.ini"
    cross_file.write_text(
        f"""[binaries]
c = {compiler!r}
ar = {ar.as_posix()!r}
strip = {strip.as_posix()!r}

[host_machine]
system = 'wasi'
cpu_family = 'wasm32'
cpu = 'wasm32'
endian = 'little'

[properties]
needs_exe_wrapper = true

[built-in options]
c_args = ['-mllvm', '-wasm-enable-sjlj', '-mllvm', '-wasm-use-legacy-eh=false', '-D_WASI_EMULATED_PROCESS_CLOCKS']
c_link_args = ['-lsetjmp', '-lwasi-emulated-process-clocks', '-Wl,-z,stack-size=786432']
""",
        encoding="utf-8",
    )
    return cross_file


def smoke_test(source, build, bindir, env, wasmer):
    with tempfile.TemporaryDirectory(prefix="smoke-", dir=build) as tmp:
        working = Path(tmp)
        shutil.copyfile(source / "images/snake.png", working / "input.png")

        def command(tool, *args):
            if wasmer:
                return [wasmer, "run", str(bindir / f"{tool}.wasm"), "--disable-cache",
                        "--volume", f"{working}:/data", "--env", "TMPDIR=/data",
                        "--", *args]
            if os.name == "nt":
                return ["cmd.exe", "/d", "/c", "call", f"{tool}.cmd", *args]
            return [str(bindir / tool), *args]

        # Connect the encoder and decoder directly to exercise binary pipes.
        with (working / "input.png").open("rb") as src, \
                (working / "example.png").open("wb") as output:
            with subprocess.Popen(command("img2sixel"), cwd=working, env=env,
                                  stdin=src, stdout=subprocess.PIPE) as encoder, \
                    encoder.stdout:
                with subprocess.Popen(
                    command("sixel2png", "-i", "-", "-o", "-"),
                    cwd=working, env=env, stdin=encoder.stdout, stdout=output,
                ) as decoder:
                    encoder.stdout.close()
                    if decoder.wait() != 0:
                        sys.exit("sixel2png failed")
                if encoder.wait() != 0:
                    sys.exit("img2sixel failed")

        guest_dir = "/data/" if wasmer else ""
        with (working / "example.png").open("rb") as target:
            subprocess.run(
                command("lsqa", "-m", "ms-ssim", "-b",
                        "ms-ssim:min=0.98", guest_dir + "input.png", "-"),
                cwd=working, env=env, stdin=target, check=True,
            )

        # A video report creates and removes a spool in the preopened directory.
        shutil.copyfile(source / "tests/data/inputs/snake_64.six",
                        working / "input.six")
        subprocess.run(
            command("lsqa", "-Ma", "--reference-fps=1",
                    "-m", "S-CIEDE2000_max", "-b", "S-CIEDE2000_max:max=0.00001",
                    "-o", guest_dir + "report.jsonl",
                    guest_dir + "input.six", guest_dir + "input.six"),
            cwd=working, env=env, check=True,
        )
        if (not (working / "report.jsonl").stat().st_size
                or any(working.glob("libsixel-*"))):
            sys.exit("lsqa failed to write the report or left its spool behind")

        for tool in ("img2sixel", "sixel2png", "lsqa"):
            result = subprocess.run(
                command(tool, "--bad-option"), cwd=working, env=env,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            )
            if result.returncode != 2 or not result.stderr or result.stdout:
                sys.exit(f"{tool} failed the diagnostic and exit-status check")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="test the installed Wasmtime launchers")
    parser.add_argument("--check-wasmer", action="store_true",
                        help="also test the Wasm modules directly with Wasmer")
    args = parser.parse_args()

    source = Path(__file__).resolve().parents[1]
    sdk_path = os.environ.get("WASI_SDK_PATH")
    if not sdk_path:
        sys.exit("Set WASI_SDK_PATH to an extracted WASI SDK (34 or newer).")
    build = Path(os.environ.get(
        "SIXEL_WASI_BUILD_DIR", str(source / "build-examples/build-wasi")
    )).resolve()
    stage = build / "install"
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join((str(source / ".local/bin"), env["PATH"]))
    meson = shutil.which("meson", path=env["PATH"])
    if not meson:
        sys.exit("Install Meson and Ninja before running the WASI build.")
    if (args.check or args.check_wasmer) and not shutil.which(
            "wasmtime", path=env["PATH"]):
        sys.exit("Wasmtime is required for --check.")
    wasmer = shutil.which("wasmer", path=env["PATH"]) if args.check_wasmer else None
    if args.check_wasmer and not wasmer:
        sys.exit("Wasmer is required for --check-wasmer.")

    build.mkdir(parents=True, exist_ok=True)
    cross_file = make_cross_file(source, build, Path(sdk_path).resolve())
    setup = [meson, "setup"]
    if (build / "meson-private/coredata.dat").is_file():
        setup.append("--reconfigure")
    subprocess.run(
        setup + [str(build), str(source), "--cross-file", str(cross_file),
                 "--prefix", "/", "--bindir", "bin", "--libdir", "lib",
                 "--buildtype", "release", "--default-library", "static",
                 "--auto-features", "disabled", "-Dthreads=disabled",
                 "-Dtests=false", "-Db_vscrt=none"],
        env=env, check=True,
    )
    subprocess.run([meson, "compile", "-C", str(build)], env=env, check=True)
    subprocess.run([meson, "install", "-C", str(build), "--no-rebuild",
                    "--destdir", str(stage)],
                   env=env, check=True)
    bindir = stage / "bin"
    if args.check or args.check_wasmer:
        env["PATH"] = os.pathsep.join((str(bindir), env["PATH"]))
        smoke_test(source, build, bindir, env, None)
    if wasmer:
        smoke_test(source, build, bindir, env, wasmer)
    print(f"Installed WASI commands in {bindir}")


if __name__ == "__main__":
    main()
