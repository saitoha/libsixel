"""Shared helpers for libsixel ctypes bindings."""

from __future__ import annotations

import threading
from typing import Optional

from .loader import LibraryLoadError, load_ctypes_library

__all__ = [
    "LibraryLoadError",
    "ensure_library",
    "get_library",
    "get_library_path",
    "reset",
]

_LOCK = threading.RLock()
_LIBRARY = None
_LIBRARY_PATH: Optional[str] = None


def ensure_library(path: Optional[str] = None):
    """Load the libsixel shared library if it has not been loaded yet."""
    global _LIBRARY, _LIBRARY_PATH
    with _LOCK:
        if _LIBRARY is not None:
            return _LIBRARY
        library, resolved_path = load_ctypes_library(path)
        _LIBRARY = library
        _LIBRARY_PATH = resolved_path
        return _LIBRARY


def get_library():
    """Return the cached ctypes handle for libsixel."""
    with _LOCK:
        return _LIBRARY


def get_library_path() -> Optional[str]:
    """Return the absolute path of the loaded libsixel library."""
    with _LOCK:
        return _LIBRARY_PATH


def reset():
    """Forget the cached library handle. Intended for tests."""
    global _LIBRARY, _LIBRARY_PATH
    with _LOCK:
        _LIBRARY = None
        _LIBRARY_PATH = None
