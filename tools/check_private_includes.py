#!/usr/bin/env python3
"""Validate that public trees do not include private src headers."""

from __future__ import annotations

import pathlib
import re
import subprocess
import sys
from typing import List, Tuple


def _repo_root() -> pathlib.Path:
    result = subprocess.run(
        ["git", "rev-parse", "--show-toplevel"],
        check=True,
        capture_output=True,
        text=True,
    )
    return pathlib.Path(result.stdout.strip())


def _tracked_files(root: pathlib.Path) -> List[pathlib.Path]:
    result = subprocess.run(
        ["git", "ls-files", "-z"],
        check=True,
        capture_output=True,
    )
    raw = result.stdout.split(b"\0")
    files = []
    for entry in raw:
        if not entry:
            continue
        files.append(root / entry.decode("utf-8"))
    return files


def _scan_file(path: pathlib.Path, pattern: re.Pattern[str]) -> List[Tuple[int, str]]:
    try:
        content = path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        content = path.read_text(encoding="utf-8", errors="replace")
    matches: List[Tuple[int, str]] = []
    for idx, line in enumerate(content.splitlines(), start=1):
        if pattern.search(line):
            matches.append((idx, line.rstrip()))
    return matches


def main() -> int:
    root = _repo_root()
    pattern = re.compile(r"^\s*#\s*include\s+\"\.\./src/[^\"]+\"")
    offenders: List[Tuple[pathlib.Path, List[Tuple[int, str]]]] = []
    for path in _tracked_files(root):
        if not path.is_file():
            continue
        hits = _scan_file(path, pattern)
        if hits:
            offenders.append((path, hits))
    if not offenders:
        return 0
    print(
        "error: detected includes that pull private src headers into public trees",
        file=sys.stderr,
    )
    print(
        "hint: public tools should depend on exported headers such as include/sixel.h",
        file=sys.stderr,
    )
    print(
        "hint: remove the '../src/*.h' include or stub the required helpers locally",
        file=sys.stderr,
    )
    for path, hits in offenders:
        rel = path.relative_to(root)
        for line_no, line in hits:
            print(f"  {rel}:{line_no}: {line}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
