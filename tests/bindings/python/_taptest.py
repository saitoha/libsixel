#!/usr/bin/env python3
"""Shared TAP helpers for Python binding tests."""

from __future__ import annotations

import io
import sys
import traceback
from contextlib import redirect_stdout
from typing import Callable


def _skip_all(message: str) -> int:
    print(f"1..0 # SKIP {message}")
    return 0


def run_embedded_tap_test(
    description: str, test_func: Callable[[], None]
) -> int:
    output = io.StringIO()
    code = 0
    detail = ""

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
