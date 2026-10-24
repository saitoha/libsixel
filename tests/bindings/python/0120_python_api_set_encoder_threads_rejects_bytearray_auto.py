#!/usr/bin/env python3
"""TAP test that set_encoder_threads rejects bytearray auto input."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'set_encoder_threads rejects bytearray auto input'


def test_0120_python_api_set_encoder_threads_rejects_bytearray_auto() -> None:
    try:
        from libsixel_wheel import sixel_set_encoder_threads
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    try:
        sixel_set_encoder_threads(bytearray(b'auto'))
    except ValueError:
        print('set_encoder_threads bytearray auto rejection verified')
        return

    raise SystemExit('set_encoder_threads accepted bytearray auto input')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0120_python_api_set_encoder_threads_rejects_bytearray_auto,
    ))
