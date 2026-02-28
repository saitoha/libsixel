#!/usr/bin/env python3
"""Shared TAP helpers for Python binding tests."""

from __future__ import annotations

import sys
import traceback
from contextlib import redirect_stdout
from typing import Callable


def _skip_all(message: str) -> int:
    print(f"1..0 # SKIP {message}")
    return 0


class _StdoutToStderr:
    """Stream stdout to stderr while capturing the skip marker line only."""

    def __init__(self) -> None:
        self.marker = ""
        self._pending = ""

    def write(self, chunk: str) -> int:
        sys.stderr.write(chunk)
        self._pending += chunk
        while "\n" in self._pending:
            line, self._pending = self._pending.split("\n", 1)
            self._capture_marker(line)
        return len(chunk)

    def flush(self) -> None:
        sys.stderr.flush()

    def finish(self) -> None:
        if self._pending:
            self._capture_marker(self._pending)
            self._pending = ""

    def _capture_marker(self, line: str) -> None:
        if self.marker:
            return
        if line.startswith("SKIP_LIBSIXEL_LOAD:"):
            self.marker = line[len("SKIP_LIBSIXEL_LOAD:"):]


def run_embedded_tap_test(
    description: str, test_func: Callable[[], None]
) -> int:
    sink = _StdoutToStderr()
    code = 0
    detail = ""

    try:
        with redirect_stdout(sink):
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

    sink.finish()

    if code == 0:
        print("1..1")
        print(f"ok 1 - {description}")
        return 0

    if code == 2 and sink.marker:
        return _skip_all(f"libsixel failed to load: {sink.marker}")

    print("1..1")
    print(f"not ok 1 - {description}")
    if detail:
        for line in detail.splitlines():
            print(f"# {line}")
    return 0
