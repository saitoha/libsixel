#!/usr/bin/env python3
"""TAP test that decoder setopt rejects bytearray option flag input."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'decoder setopt rejects bytearray option flag input'


def test_0118_python_api_decoder_setopt_rejects_bytearray_flag() -> None:
    try:
        from libsixel_wheel import sixel_decoder_new
        from libsixel_wheel import sixel_decoder_setopt
        from libsixel_wheel import sixel_decoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    decoder = sixel_decoder_new()

    try:
        sixel_decoder_setopt(decoder, bytearray(b'i'), 'dummy.png')
    except RuntimeError:
        sixel_decoder_unref(decoder)
        print('decoder bytearray option flag rejection verified')
        return

    sixel_decoder_unref(decoder)
    raise AssertionError('decoder accepted bytearray option flag input')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0118_python_api_decoder_setopt_rejects_bytearray_flag,
    ))
