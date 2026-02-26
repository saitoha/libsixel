#!/usr/bin/env python3
"""TAP test for invalid decoder option flag width."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'decoder setopt rejects multi-character option flags'


def test_0076_python_api_decoder_setopt_invalid_flag_length() -> None:
    try:
        from libsixel_wheel import sixel_decoder_new
        from libsixel_wheel import sixel_decoder_setopt
        from libsixel_wheel import sixel_decoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    decoder = sixel_decoder_new()

    try:
        sixel_decoder_setopt(decoder, 'xy', 'dummy.png')
    except TypeError:
        sixel_decoder_unref(decoder)
        print('decoder multi-character flag rejection verified')
        return

    sixel_decoder_unref(decoder)
    raise AssertionError('decoder accepted multi-character option flag')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0076_python_api_decoder_setopt_invalid_flag_length,
    ))
