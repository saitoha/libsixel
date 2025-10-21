"""ctypes loader for libsixel shared libraries."""

from __future__ import annotations

import ctypes
import os
import sys
from ctypes.util import find_library
from typing import Iterable, List, Optional, Tuple

__all__ = ["LibraryLoadError", "candidate_names", "load_ctypes_library"]


class LibraryLoadError(RuntimeError):
    """Raised when the libsixel shared library cannot be located."""


def candidate_names() -> List[str]:
    """Return platform-specific candidate library file names."""
    platform = sys.platform
    if platform.startswith("win"):
        return ["libsixel-1.dll", "libsixel.dll"]
    if platform == "darwin":
        return ["libsixel.1.dylib", "libsixel.dylib"]
    if platform.startswith("cygwin") or platform.startswith("msys"):
        return ["cygsixel-1.dll", "msys-sixel-1.dll", "libsixel-1.dll", "libsixel.dll"]
    return ["libsixel-1.so", "libsixel.so.1", "libsixel.so"]


def _expand_candidates(root: str) -> Iterable[str]:
    entries: List[str] = []
    for name in candidate_names():
        entries.append(os.path.join(root, name))
    return entries


def _resolve_default_path() -> Optional[str]:
    loader_names = candidate_names()
    for name in loader_names:
        resolved = find_library(name)
        if resolved:
            return resolved
    search_paths = []
    env_keys = ["LIBSIXEL_PATH", "LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH", "PATH"]
    for key in env_keys:
        value = os.environ.get(key)
        if value:
            parts = value.split(os.pathsep)
            search_paths.extend(parts)
    for directory in search_paths:
        if not directory:
            continue
        for candidate in _expand_candidates(directory):
            if os.path.exists(candidate):
                return candidate
    return None


def load_ctypes_library(path: Optional[str] = None) -> Tuple[ctypes.CDLL, str]:
    """Load libsixel via ctypes and return the handle and resolved path."""
    resolved_path = path
    if resolved_path:
        if not os.path.isabs(resolved_path):
            resolved_path = os.path.abspath(resolved_path)
        if not os.path.exists(resolved_path):
            raise LibraryLoadError(f"Specified libsixel library does not exist: {resolved_path}")
    else:
        resolved_path = _resolve_default_path()
        if not resolved_path:
            names = ", ".join(candidate_names())
            raise LibraryLoadError(
                f"Unable to locate libsixel shared library. Tried: {names}"
            )
    handle = ctypes.CDLL(resolved_path)
    return handle, resolved_path
