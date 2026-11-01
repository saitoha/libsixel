#!/usr/bin/env python3
"""TAP test that decoder decode rejects integer infile input."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'decoder decode rejects integer infile input'


def test_0109_python_api_decoder_decode_rejects_integer_infile() -> None:
    try:
        from libsixel_wheel import sixel_decoder_decode
        from libsixel_wheel import sixel_decoder_new
        from libsixel_wheel import sixel_decoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    decoder = sixel_decoder_new()

    try:
        sixel_decoder_decode(decoder, 12345)
    except RuntimeError:
        sixel_decoder_unref(decoder)
        print('decoder integer infile rejection verified')
        return

    sixel_decoder_unref(decoder)
    raise SystemExit('decoder accepted integer infile input')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0109_python_api_decoder_decode_rejects_integer_infile,
    ))
