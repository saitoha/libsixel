"""Load production discovery rules into a small, isolated make fixture."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile


class Fixture:
    def __init__(self, root, openvms=False):
        self.root = Path(root).resolve()
        self.temp = tempfile.TemporaryDirectory(prefix="libsixel-discovery-")
        self.work = Path(self.temp.name)
        self.source = self.work / "source tree"
        self.source.mkdir()
        self.make = (os.environ.get("SIXEL_STATICCHECK_MAKE") or
                     shutil.which("gmake") or shutil.which("make"))
        if not self.make:
            raise RuntimeError("GNU make is required")
        self.env = {k: v for k, v in os.environ.items()
                    if k not in ("MAKEFLAGS", "MFLAGS", "MAKELEVEL",
                                 "TESTS", "SIXEL_INSTALLCHECK_GENERIC")}
        text = (self.root / "tests/Makefile.am").read_text()
        block = text.split("# Keep OpenVMS inventories", 1)[1]
        block = "# Keep OpenVMS inventories" + block.split("XFAIL_TESTS =", 1)[0]
        lines = []
        enabled = True
        # These are the actual Automake conditional branches, not a second
        # implementation of discovery. Ordinary and file-list paths share
        # the real scanner and production make assignments/recipe.
        for line in block.splitlines():
            if line == "if OPENVMS_GNV":
                enabled = openvms
            elif line == "if !OPENVMS_GNV":
                enabled = not openvms
            elif line == "endif":
                enabled = True
            elif enabled:
                lines.append(line)
        recipe = "\n".join(lines) + "\n"
        if openvms:
            rule = text.split(".PHONY: sixel-check-test-list\n", 1)[1]
            recipe += ".PHONY: sixel-check-test-list\n"
            recipe += rule.split("\nsixel-check-test-logs:", 1)[0] + "\n"
            recipe += "inventory: sixel-check-test-list\n"
            recipe += "\t@cat \"$(OPENVMS_CHECK_TESTS_LIST)\"\n"
        else:
            recipe += "inventory:\n\t@printf '%s\\n' $(TESTS)\n"
        (self.work / "Makefile").write_text(
            f"top_srcdir = {self.root}\nsrcdir = {self.source}\n"
            "builddir = .\nSHELL = /bin/sh\n" + recipe)

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.temp.cleanup()

    def add(self, name):
        path = self.source / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.touch()
        return path

    def run(self, *args, success=True):
        result = subprocess.run(
            [self.make, "--no-print-directory", "inventory", *args],
            cwd=self.work, env=self.env, text=True, capture_output=True)
        if (result.returncode == 0) != success:
            raise AssertionError(result.stdout + result.stderr)
        return result.stdout.split()

    def fail_scan(self):
        bindir = self.work / "bin"
        bindir.mkdir()
        script = bindir / "sort"
        # Emit a plausible partial result before failing: checking only an
        # empty inventory would let a truncated scan silently pass.
        script.write_text("#!/bin/sh\nprintf './sample/existing/0001_existing.t\\n'\nexit 1\n")
        script.chmod(0o755)
        self.env["PATH"] = str(bindir) + os.pathsep + self.env["PATH"]
