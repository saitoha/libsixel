"""Locate vendored libsixel binaries packaged with the wheel."""

from __future__ import annotations

import os
import sys
from importlib import resources
from typing import Iterable, List

__all__ = ["locate"]


def _candidate_directories() -> Iterable[str]:
    return (".binaries", ".dylibs", ".libs")


def _candidate_filenames() -> List[str]:
    platform = sys.platform
    if platform.startswith("win"):
        return ["libsixel-1.dll", "libsixel.dll"]
    if platform == "darwin":
        return ["libsixel.1.dylib", "libsixel.dylib"]
    if platform.startswith("cygwin") or platform.startswith("msys"):
        return ["cygsixel-1.dll", "msys-sixel-1.dll", "libsixel-1.dll", "libsixel.dll"]
    return ["libsixel-1.so", "libsixel.so.1", "libsixel.so"]


def locate() -> str:
    """Return the absolute path to the vendored libsixel shared library."""
    package_root = resources.files(__package__)
    filenames = _candidate_filenames()
    for directory in _candidate_directories():
        try:
            root = package_root.joinpath(directory)
        except FileNotFoundError:
            continue
        for filename in filenames:
            candidate = root.joinpath(filename)
            if not candidate.is_file():
                continue
            with resources.as_file(candidate) as resolved:
                resolved_path = os.fspath(resolved)
                if os.path.exists(resolved_path):
                    return resolved_path
    searched = []
    for directory in _candidate_directories():
        searched.append(os.path.join(directory, "*"))
    details = ", ".join(searched)
    raise FileNotFoundError(
        f"Unable to find libsixel binary in package resources. Looked under: {details}"
    )
