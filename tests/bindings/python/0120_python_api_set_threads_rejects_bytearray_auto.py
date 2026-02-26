#!/usr/bin/env python3
"""TAP test for bytearray auto rejection in set_threads."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'set_threads rejects bytearray auto input'


def test_0123_python_api_set_threads_rejects_bytearray_auto() -> None:
    try:
        from libsixel_wheel import sixel_set_threads
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    try:
        sixel_set_threads(bytearray(b'auto'))
    except ValueError:
        print('set_threads bytearray auto rejection verified')
        return

    raise AssertionError('set_threads accepted bytearray auto input')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0123_python_api_set_threads_rejects_bytearray_auto,
    ))
