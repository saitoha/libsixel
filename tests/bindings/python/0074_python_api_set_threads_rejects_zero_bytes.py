#!/usr/bin/env python3
"""TAP test that set_threads rejects zero value for byte input."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'set_threads rejects zero value for byte input'


def test_0074_python_api_set_threads_rejects_zero_bytes() -> None:
    try:
        from libsixel_wheel import sixel_set_threads
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    try:
        sixel_set_threads(b'0')
    except ValueError:
        print('set_threads zero-byte rejection verified')
        return

    raise SystemExit('set_threads accepted zero-byte input')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0074_python_api_set_threads_rejects_zero_bytes,
    ))
