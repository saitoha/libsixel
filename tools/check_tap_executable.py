#!/usr/bin/env python3
"""Check executable shell TAP sources without changing the tree or index."""

import os
from pathlib import Path
import stat
import subprocess
import sys


def git(root, *args):
    try:
        return subprocess.run(
            ["git", "-C", str(root), *args], capture_output=True, check=False
        )
    except FileNotFoundError:
        return None


def inspect(root, filesystem_modes):
    """Use real POSIX permissions, plus Git modes on mode-blind checkouts."""
    top = git(root, "rev-parse", "--show-toplevel")
    repository = (
        top is not None and top.returncode == 0
        and Path(os.fsdecode(top.stdout).strip()).resolve() == root
    )
    paths = {}
    index_modes = False
    if repository:
        setting = git(root, "config", "--bool", "core.filemode")
        index_modes = not filesystem_modes or setting.stdout.strip() == b"false"
        tracked = git(root, "ls-files", "--stage", "-z", "--", "tests/")
        untracked = git(
            root, "ls-files", "--others", "--exclude-standard", "-z",
            "--", "tests/"
        )
        if tracked.returncode or untracked.returncode:
            raise RuntimeError("cannot read Git test modes")
        for record in tracked.stdout.split(b"\0"):
            if not record:
                continue
            metadata, name = record.split(b"\t", 1)
            mode, unused_blob, stage = metadata.split()
            name = os.fsdecode(name)
            if name.endswith(".t"):
                if stage != b"0":
                    raise RuntimeError("unmerged shell TAP: " + name)
                paths[name] = mode.decode("ascii")
        for name in untracked.stdout.split(b"\0"):
            if name.endswith(b".t"):
                paths[os.fsdecode(name)] = None
    else:
        if not filesystem_modes:
            return None, ["no Git modes or POSIX filesystem permissions"]
        for directory, directories, files in os.walk(root / "tests"):
            directories[:] = [
                name for name in directories
                if not name.startswith(".") and name not in ("_artifacts", "data")
            ]
            for name in files:
                if name.endswith(".t"):
                    path = Path(directory) / name
                    # macOS tar may carry AppleDouble metadata beside a TAP.
                    # A name alone is insufficient to exclude a shell source.
                    if name.startswith("._"):
                        with path.open("rb") as stream:
                            if stream.read(4) == b"\x00\x05\x16\x07":
                                continue
                    paths[str(path.relative_to(root))] = None

    failures = []
    checked = 0
    for name, mode in sorted(paths.items()):
        path = root / name
        try:
            current = path.lstat()
        except FileNotFoundError:
            continue  # An unstaged deletion is outside the current corpus.
        checked += 1
        if not stat.S_ISREG(current.st_mode):
            failures.append(name + ": expected a regular shell TAP file")
            continue
        if filesystem_modes and not current.st_mode & stat.S_IXUSR:
            failures.append(name + ": missing owner execute bit; use chmod 755")
        if index_modes and mode != "100755":
            failures.append(
                name + ": expected Git mode 100755; add the file and use "
                "git update-index --chmod=+x -- <path>"
            )
    if not checked:
        failures.append("no shell TAP sources found under tests/")
    return checked, failures


def main():
    root = Path(sys.argv[1]).resolve()
    try:
        checked, failures = inspect(root, os.name != "nt")
    except (OSError, RuntimeError, ValueError) as error:
        print(str(error), file=sys.stderr)
        return 1
    for message in failures:
        print(message, file=sys.stderr)
    if checked is None:
        return 77
    if failures:
        return 1
    print("# executable shell TAP sources: {}".format(checked))
    return 0


if __name__ == "__main__":
    sys.exit(main())
