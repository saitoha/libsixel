#!/usr/bin/env python3
"""TAP test that set_threads rejects non-UTF-8 byte input."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'set_threads rejects non-UTF-8 byte input'


def test_0076_python_api_set_threads_rejects_non_utf8_bytes() -> None:
    try:
        from libsixel_wheel import sixel_set_threads
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    try:
        sixel_set_threads(b'\xff')
    except ValueError:
        print('set_threads non-UTF-8-byte rejection verified')
        return

    raise SystemExit('set_threads accepted non-UTF-8 byte input')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0076_python_api_set_threads_rejects_non_utf8_bytes,
    ))
