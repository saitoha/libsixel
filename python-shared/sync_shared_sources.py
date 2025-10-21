#!/usr/bin/env python3
"""Synchronize shared ctypes sources into package directories."""
from __future__ import annotations

import argparse
import pathlib
import shutil
import sys
from typing import Iterable


def _copy_tree(src: pathlib.Path, dst: pathlib.Path) -> None:
    """Copy *src* directory contents into *dst*, replacing the target."""
    if dst.exists():
        shutil.rmtree(dst)
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(src, dst)


def sync_shared_sources(root: pathlib.Path) -> None:
    """Replicate shared ctypes helpers into consuming packages."""
    shared_dir = root / 'python-shared' / 'src' / '_libsixel_common'
    if not shared_dir.is_dir():
        raise FileNotFoundError(f"shared sources not found: {shared_dir}")

    destinations: Iterable[pathlib.Path] = (
        root / 'python' / 'libsixel' / '_libsixel_common',
        root / 'python-wheel' / 'src' / 'libsixel_wheel' / '_libsixel_common',
    )
    for dest in destinations:
        _copy_tree(shared_dir, dest)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        'root',
        nargs='?',
        default=None,
        help='project root containing python-shared/',
    )
    ns = parser.parse_args(argv[1:])

    if ns.root is None:
        root = pathlib.Path(__file__).resolve().parents[1]
    else:
        root = pathlib.Path(ns.root).resolve()
    sync_shared_sources(root)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
