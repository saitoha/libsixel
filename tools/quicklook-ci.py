#!/usr/bin/env python3
"""Explicit system-only lifecycle plan for disposable GitHub macOS runners.

This does not activate a user or restore an existing installation. Refuse that
state instead. The existing native lifecycle owns status/cleanup journaling.
"""
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys


ROOTS = (
    "/Applications/SixelQuickLookHost.app",
    "/Applications/SixelPreviewBridge.app",
    "/Library/Application Support/libsixel/QuickLook",
    "/private/var/run/libsixel-quicklook.lock",
    "/private/var/run/libsixel-quicklook-managed.lock",
)
RECEIPT = "com.saitoha.libsixel.quicklook.pkg"
USER_SKIP = "1..0 # SKIP system-only: no explicit participating user context"
OFF_SKIP = "1..0 # SKIP requires quicklook_extension=0"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def runner_guard():
    require(sys.platform == "darwin" and os.geteuid() != 0 and
            os.environ.get("GITHUB_ACTIONS") == "true" and
            os.environ.get("RUNNER_ENVIRONMENT") == "github-hosted" and
            os.environ.get("RUNNER_OS") == "macOS",
            "requires an unprivileged disposable GitHub-hosted macOS job")
    for name in ("DESTDIR", "SIXEL_INSTALLCHECK_DESTDIR",
                 "QUICKLOOK_USER_CONTEXT", "QUICKLOOK_OBSERVER_PATH",
                 "QUICKLOOK_EXPECTED_DIR", "QUICKLOOK_DRY_RUN",
                 "MESON_INSTALL_DRY_RUN"):
        require(not os.environ.get(name), "unexpected override: " + name)


def execute(command, capture=False):
    result = subprocess.run(command, text=True, check=True,
                            stdout=subprocess.PIPE if capture else None)
    return result.stdout


def absent():
    require(not any(os.path.lexists(path) for path in ROOTS),
            "pre-existing/residual payload or operation lock; no adoption")
    receipts = execute(["/usr/sbin/pkgutil", "--pkgs"], True).splitlines()
    require(RECEIPT not in receipts, "pre-existing/residual receipt")


def validate_results(sources, results, installed):
    """Require each registered case; user-only absence is not system success."""
    require(sources and set(sources) == set(results),
            "empty, missing or unexpected result inventory")
    system_pass = 0
    for name, source in sources.items():
        result, output = results[name]
        text = source.read_text()
        user_only = installed and USER_SKIP in text
        off_only = not installed and OFF_SKIP in text
        if result in ("PASS", "OK"):
            if not user_only and not off_only:
                system_pass += 1
        else:
            require(result == "SKIP" and (
                    (user_only and USER_SKIP in output.splitlines()) or
                    (off_only and OFF_SKIP in output.splitlines())),
                    "required observation did not pass: " + name)
    require(system_pass > 0, "all-SKIP is not installation evidence")


def observe(config, installed):
    source = Path(config["source"])
    build = Path(config["build"])
    suite = "quicklook-installed" if installed else "quicklook-package"
    sources = {f"extension/{suite}/{p.name}": p for p in
               (source / "tests/extension" / suite).glob("????_*.t")}
    require(sources, "registered observation inventory is empty")
    results = {}
    if config["system"] == "autotools":
        if installed:
            execute([config["make"], "-C", str(build),
                     "installcheck-quicklook"])
            report = build / "quicklook-extension/installcheck"
        else:
            # Request existing TAP log targets, not check_PROGRAMS/fixtures.
            tests = build / "tests"
            for name in sources:
                for suffix in (".log", ".trs"):
                    (tests / Path(name).with_suffix(suffix)).unlink(
                        missing_ok=True)
            execute([config["make"], "-C", str(tests),
                     *[str(Path(n).with_suffix(".log")) for n in sources]])
            report = tests / "extension" / suite
        for name in sources:
            stem = Path(name).stem
            trs = (report / (stem + ".trs")).read_text()
            status = re.findall(r"^:global-test-result: (\S+)$", trs, re.M)
            require(len(status) == 1, "invalid TAP result: " + name)
            results[name] = (status[0], (report / (stem + ".log")).read_text())
    else:
        if installed:
            execute([config["meson"], "compile", "-C", str(build),
                     "installcheck-quicklook"])
            log = build / "meson-logs/quicklook-installed-installcheck.json"
        else:
            execute([config["meson"], "test", "-C", str(build),
                     "--no-rebuild", "--logbase", "quicklook-ci-package",
                     "--print-errorlogs", "extension/quicklook-package/*"])
            log = build / "meson-logs/quicklook-ci-package.json"
        for line in log.read_text().splitlines():
            row = json.loads(line)
            name = re.sub(r"^(?:.* - )?libsixel:", "",
                          row["name"].rsplit(" / ", 1)[-1])
            require(name not in results, "duplicate result: " + name)
            results[name] = (row["result"], row.get("stdout", ""))
    validate_results(sources, results, installed)


def phase(name, config):
    runner_guard()
    directory = Path(config["record"])
    tool = config["tool"]
    if name == "check":
        require(platform.machine() in ("arm64", "x86_64"),
                "unsupported native runner architecture")
        execute(["/usr/bin/sudo", "-n", "/usr/bin/true"])
        absent()
        snapshot = json.loads(execute([tool, "user-state"], True))
        for key in ("host-ls", "bridge-ls", "preview-plugin",
                    "thumbnail-plugin"):
            require(snapshot[key] == [], "pre-existing user candidate: " + key)
        for key in ("six-default", "sixel-default"):
            handler = snapshot[key]["handler"]
            require(not handler or not handler.lower().startswith(
                "com.saitoha.libsixel"), "pre-existing project default: " + key)
        print("Quick Look CI runner:", platform.platform(),
              platform.machine(), snapshot["session"], flush=True)
        # Capture UID/session and all defaults; GUI capability is recorded,
        # not required or represented as tested by this system-only job.
        (directory / "before.json").write_text(json.dumps(snapshot, indent=2))
        observe(config, False)
    elif name == "observe":
        observe(config, True)
    elif name == "verify-restored":
        absent()
        snapshot = json.loads(execute([tool, "user-state"], True))
        (directory / "after.json").write_text(json.dumps(snapshot, indent=2))
        require(snapshot == json.loads((directory / "before.json").read_text()),
                "user state differs; do not reset registration/default/cache")
    else:
        raise ValueError("unknown CI phase")


def prepare(system, build_arg, record_arg):
    runner_guard()
    require(system in ("autotools", "meson"), "unknown build system")
    source = Path(__file__).resolve().parent.parent
    build = Path(build_arg).resolve()
    record = Path(record_arg).resolve()
    record.mkdir(mode=0o700, parents=True, exist_ok=False)
    (record / "run").mkdir(mode=0o700)
    tool = str(build / "quicklook-extension/sixel-quicklook-package")
    distribution = str(build / "quicklook-extension/package")
    config = dict(source=str(source), build=str(build), record=str(record),
                  tool=tool, system=system, make=shutil.which("make"),
                  meson=shutil.which("meson"))
    # Preparation precedes the shared action's dependency installation.
    # Resolve Meson in its stage process instead if not installed yet.
    config["make"] = config["make"] or "make"
    config["meson"] = config["meson"] or "meson"
    config_path = record / "config.json"
    config_path.write_text(json.dumps(config, indent=2))
    command = [sys.executable, str(Path(__file__).resolve())]
    plan = dict(format=1, explicitLifecycle=True)
    for name in ("check", "observe", "verify-restored"):
        plan[name] = command + [name, str(config_path)]
    for name in ("activate", "deactivate", "restore"):
        # Empty initial state, no participating users, no user mutation.
        # These are explicit no-ops, not evidence of GUI user restoration.
        plan[name] = ["/usr/bin/true"]
    for name, action in (("install", "install"), ("remove", "uninstall")):
        plan[name] = ["/usr/bin/sudo", "-n", "/bin/bash",
                      str(source / "tools/quicklook-extension.bash"), action,
                      "--package-dir", distribution, "--tool", tool]
        if name == "remove":
            plan[name].append("--users-deactivated")
    (record / "plan.json").write_text(json.dumps(plan, indent=2))
    with open(os.environ["GITHUB_ENV"], "a") as env:
        env.write(f"QUICKLOOK_LIFECYCLE_PLAN={record / 'plan.json'}\n"
                  f"QUICKLOOK_LIFECYCLE_RECORD={record / 'run'}\n")


def main(argv):
    try:
        if len(argv) == 5 and argv[1] == "plan":
            prepare(*argv[2:])
        elif len(argv) == 3:
            phase(argv[1], json.loads(Path(argv[2]).read_text()))
        else:
            raise ValueError("expected plan SYSTEM BUILD RECORD or PHASE CONFIG")
        return 0
    except subprocess.CalledProcessError as error:
        return error.returncode if error.returncode > 0 else 128-error.returncode
    except (OSError, ValueError, KeyError) as error:
        print("Quick Look CI refused/incomplete:", error, file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
