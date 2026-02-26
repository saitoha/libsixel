#!/usr/bin/env python3
"""TAP test that decoder decode rejects bytearray infile input."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'decoder decode rejects bytearray infile input'


def test_0117_python_api_decoder_decode_rejects_bytearray_infile() -> None:
    try:
        from libsixel_wheel import sixel_decoder_decode
        from libsixel_wheel import sixel_decoder_new
        from libsixel_wheel import sixel_decoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    decoder = sixel_decoder_new()

    try:
        sixel_decoder_decode(decoder, bytearray(b'dummy.six'))
    except RuntimeError:
        sixel_decoder_unref(decoder)
        print('decoder bytearray infile rejection verified')
        return

    sixel_decoder_unref(decoder)
    raise AssertionError('decoder accepted bytearray infile input')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0117_python_api_decoder_decode_rejects_bytearray_infile,
    ))
