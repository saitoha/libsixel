#!/usr/bin/env python3
"""TAP test that decoder setopt rejects None option flag."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'decoder setopt rejects None option flag'


def test_0094_python_api_decoder_setopt_rejects_none_flag() -> None:
    try:
        from libsixel_wheel import sixel_decoder_new
        from libsixel_wheel import sixel_decoder_setopt
        from libsixel_wheel import sixel_decoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    decoder = sixel_decoder_new()

    try:
        sixel_decoder_setopt(decoder, None, 'foo')
    except TypeError:
        sixel_decoder_unref(decoder)
        print('decoder None option flag rejection verified')
        return

    sixel_decoder_unref(decoder)
    raise AssertionError('decoder accepted None option flag')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0094_python_api_decoder_setopt_rejects_none_flag,
    ))
