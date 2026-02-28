#!/usr/bin/env python3
"""TAP test that decoder decode rejects non-pathlike infile argument."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'decoder decode rejects non-pathlike infile argument'


def test_0136_python_api_decoder_decode_rejects_nonpathlike_infile() -> None:
    try:
        from libsixel_wheel import sixel_decoder_decode
        from libsixel_wheel import sixel_decoder_new
        from libsixel_wheel import sixel_decoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    decoder = sixel_decoder_new()
    try:
        sixel_decoder_decode(decoder, object())
    except (RuntimeError, ValueError, TypeError):
        sixel_decoder_unref(decoder)
        print('decoder non-pathlike infile rejection verified')
        return

    sixel_decoder_unref(decoder)
    raise SystemExit('decoder accepted non-pathlike infile object')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0136_python_api_decoder_decode_rejects_nonpathlike_infile,
    ))
