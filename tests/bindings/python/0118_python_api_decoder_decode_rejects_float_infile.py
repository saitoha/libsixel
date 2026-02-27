#!/usr/bin/env python3
"""TAP test that decoder decode rejects float infile input."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'decoder decode rejects float infile input'


def test_0121_python_api_decoder_decode_rejects_float_infile() -> None:
    try:
        from libsixel_wheel import sixel_decoder_decode
        from libsixel_wheel import sixel_decoder_new
        from libsixel_wheel import sixel_decoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    decoder = sixel_decoder_new()

    try:
        sixel_decoder_decode(decoder, 1.5)
    except RuntimeError:
        sixel_decoder_unref(decoder)
        print('decoder float infile rejection verified')
        return

    sixel_decoder_unref(decoder)
    raise SystemExit('decoder accepted float infile input')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0121_python_api_decoder_decode_rejects_float_infile,
    ))
