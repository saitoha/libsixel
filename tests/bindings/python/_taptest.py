#!/usr/bin/env python3
"""Shared TAP helpers for Python binding tests."""

from __future__ import annotations

import io
import os
import pathlib
import sys
import traceback
from contextlib import redirect_stdout
from typing import Callable


def _skip_all(message: str) -> int:
    print(f"1..0 # SKIP {message}")
    return 0


def _prepend_library_path(var_name: str, libdir: str) -> None:
    current = os.environ.get(var_name, "")
    os.environ[var_name] = f"{libdir}:{current}" if current else libdir


def _ensure_python_runtime() -> bool:
    if os.environ.get("ENABLE_PYTHON", "0") != "1":
        _skip_all("python bindings are disabled in this build")
        return False

    python_bin = os.environ.get("SIXEL_TEST_PYTHON", "")
    if not python_bin:
        _skip_all("python wheel test interpreter is unavailable")
        return False

    candidate = pathlib.Path(python_bin)
    if not candidate.exists():
        _skip_all("python wheel test interpreter is unavailable")
        return False

    current = pathlib.Path(sys.executable).resolve()
    expected = candidate.resolve()
    if current != expected:
        os.execv(str(expected), [str(expected), *sys.argv])
    return True


def _prepare_runtime_env() -> None:
    top_builddir = os.environ.get("TOP_BUILDDIR", "")
    libdir = os.environ.get("LIBSIXEL_LIBDIR", "")
    if not libdir:
        if top_builddir:
            preferred = pathlib.Path(top_builddir) / "src" / ".libs"
            fallback = pathlib.Path(top_builddir) / "src"
            libdir = str(preferred if preferred.is_dir() else fallback)
        else:
            libdir = "src"
    os.environ["LIBSIXEL_LIBDIR"] = libdir
    _prepend_library_path("LD_LIBRARY_PATH", libdir)
    _prepend_library_path("DYLD_LIBRARY_PATH", libdir)


def run_embedded_tap_test(
    description: str, argv: list[str], test_func: Callable[[], None]
) -> int:
    if not _ensure_python_runtime():
        return 0

    _prepare_runtime_env()

    output = io.StringIO()
    code = 0
    detail = ""

    old_argv = list(sys.argv)
    sys.argv = [old_argv[0], *argv]
    try:
        with redirect_stdout(output):
            test_func()
    except SystemExit as exc:
        if isinstance(exc.code, int):
            code = exc.code
        elif exc.code is None:
            code = 0
        else:
            detail = str(exc.code)
            code = 1
    except Exception:  # noqa: BLE001
        detail = traceback.format_exc().strip()
        code = 1
    finally:
        sys.argv = old_argv

    raw_output = output.getvalue()
    if raw_output:
        print(raw_output, end="", file=sys.stderr)

    marker = ""
    for line in raw_output.splitlines():
        if line.startswith("SKIP_LIBSIXEL_LOAD:"):
            marker = line[len("SKIP_LIBSIXEL_LOAD:"):]
            break

    if code == 0:
        print("1..1")
        print(f"ok 1 - {description}")
        return 0

    if code == 2 and marker:
        return _skip_all(f"libsixel failed to load: {marker}")

    print("1..1")
    print(f"not ok 1 - {description}")
    if detail:
        for line in detail.splitlines():
            print(f"# {line}")
    return 0
