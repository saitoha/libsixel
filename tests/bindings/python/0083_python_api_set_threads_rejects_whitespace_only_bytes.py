#!/usr/bin/env python3
"""TAP test that set_threads rejects whitespace-only byte input."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'set_threads rejects whitespace-only byte input'


def test_0083_python_api_set_threads_rejects_whitespace_only_bytes() -> None:
    try:
        from libsixel_wheel import sixel_set_threads
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    try:
        sixel_set_threads(b'   ')
    except ValueError:
        print('set_threads whitespace-only-byte rejection verified')
        return

    raise SystemExit('set_threads accepted whitespace-only byte input')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0083_python_api_set_threads_rejects_whitespace_only_bytes,
    ))
