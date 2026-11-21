"""Run WASI SDK Clang with Meson's GNU-only linker options removed."""

import subprocess
import sys


# wasm-ld already searches archives as a group and has no ELF runtime paths.
unsupported = {"-Wl,--start-group", "-Wl,--end-group"}
args = [
    arg for arg in sys.argv[1:]
    if arg not in unsupported and not arg.startswith("-Wl,-rpath,")
]
if args[-1] == "--version":
    # Meson scans the version output for "windows" to select the COFF linker.
    # Keep the compiler version and target, and omit the SDK's host paths.
    version = subprocess.run(args, stdout=subprocess.PIPE, check=False)
    for line in version.stdout.splitlines(keepends=True):
        if not line.startswith((b"InstalledDir:", b"Configuration file:")):
            sys.stdout.buffer.write(line)
    raise SystemExit(version.returncode)
raise SystemExit(subprocess.call(args))
