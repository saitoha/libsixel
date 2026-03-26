#!/usr/bin/env python3
"""Validate that public trees do not include private src headers."""

from __future__ import annotations

import pathlib
import re
import subprocess
import sys
from typing import List, Set, Tuple


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


def _public_tree_prefixes() -> Set[str]:
    # converters/ contains the user-facing CLI tools; they should not depend on
    # private src/ headers.
    return {"converters"}


def _is_public_tree_file(root: pathlib.Path, path: pathlib.Path) -> bool:
    rel = path.relative_to(root)
    if not rel.parts:
        return False
    return rel.parts[0] in _public_tree_prefixes()


def _private_src_header_basenames(root: pathlib.Path) -> Set[str]:
    src_dir = root / "src"
    names: Set[str] = set()
    for header in src_dir.glob("*.h"):
        names.add(header.name)
    return names


def _scan_file(
    path: pathlib.Path,
    include_pattern: re.Pattern[str],
    private_headers: Set[str],
) -> List[Tuple[int, str]]:
    try:
        content = path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        content = path.read_text(encoding="utf-8", errors="replace")
    matches: List[Tuple[int, str]] = []
    for idx, line in enumerate(content.splitlines(), start=1):
        include_match = include_pattern.search(line)
        if not include_match:
            continue
        target = include_match.group(1)
        # Explicit src path includes are always private-tree violations.
        if target.startswith("../src/") or target.startswith("src/"):
            matches.append((idx, line.rstrip()))
            continue
        # Unqualified includes that resolve to src-only header names are also
        # considered private access unless a same-directory header shadows it.
        if "/" in target:
            continue
        if target not in private_headers:
            continue
        if (path.parent / target).exists():
            continue
        matches.append((idx, line.rstrip()))
    return matches


def main() -> int:
    root = _repo_root()
    include_pattern = re.compile(r"^\s*#\s*include\s+\"([^\"]+)\"")
    private_headers = _private_src_header_basenames(root)
    offenders: List[Tuple[pathlib.Path, List[Tuple[int, str]]]] = []
    for path in _tracked_files(root):
        if not path.is_file():
            continue
        if path.suffix.lower() not in {".c", ".h", ".m", ".mm"}:
            continue
        if not _is_public_tree_file(root, path):
            continue
        hits = _scan_file(path, include_pattern, private_headers)
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
