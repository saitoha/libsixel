#!/usr/bin/env python3
"""TAP test that decoder decode rejects bytes infile argument."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'decoder decode rejects bytes infile argument'


def test_0102_python_api_decoder_decode_rejects_bytes_infile() -> None:
    import os

    try:
        from libsixel_wheel import sixel_decoder_decode
        from libsixel_wheel import sixel_decoder_new
        from libsixel_wheel import sixel_decoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    infile = os.path.expandvars(
        '${TOP_SRCDIR}/tests/data/inputs/snake_64.six'
    ).encode('utf-8')

    decoder = sixel_decoder_new()
    try:
        sixel_decoder_decode(decoder, infile)
    except RuntimeError:
        sixel_decoder_unref(decoder)
        print('decoder decode bytes infile rejection verified')
        return

    sixel_decoder_unref(decoder)
    raise AssertionError('decoder decode accepted bytes infile path')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0102_python_api_decoder_decode_rejects_bytes_infile,
    ))
