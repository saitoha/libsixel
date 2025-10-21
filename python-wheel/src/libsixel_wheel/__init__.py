"""Python bindings for libsixel with vendored shared library."""

from __future__ import annotations

import importlib
import threading
from typing import Optional

from . import _bootstrap, _loader
from ._libsixel_common import LibraryLoadError, ensure_library, get_library_path

__all__ = ["LibraryLoadError", "is_ready", "lib_path", "load"]

_LOCK = threading.RLock()
_INITIALIZED = False
_API_MODULE = None


def _ensure_api() -> None:
    global _API_MODULE
    if _API_MODULE is not None:
        return
    module = importlib.import_module("._libsixel_common.api", __name__)
    names = getattr(module, "__all__", None)
    if names is None:
        names = [name for name in dir(module) if not name.startswith("_")]
    for name in names:
        globals()[name] = getattr(module, name)
        if name not in __all__:
            __all__.append(name)
    _API_MODULE = module


def _initialize() -> None:
    global _INITIALIZED
    with _LOCK:
        if _INITIALIZED:
            _ensure_api()
            return
        binary_path = _loader.locate()
        loaded = _bootstrap.load(path=binary_path)
        if not loaded:
            raise RuntimeError("_bootstrap.load() returned False without raising")
        ensure_library(binary_path)
        _ensure_api()
        _INITIALIZED = True


def load() -> None:
    """Ensure the vendored libsixel library is loaded."""

    _initialize()


def is_ready() -> bool:
    """Return True if the vendored library has been successfully loaded."""

    _initialize()
    return get_library_path() is not None


def lib_path() -> Optional[str]:
    """Return the absolute path to the vendored libsixel shared library."""

    _initialize()
    return get_library_path()


_initialize()
