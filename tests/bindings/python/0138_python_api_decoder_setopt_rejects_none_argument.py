#!/usr/bin/env python3
"""TAP test that decoder setopt rejects None argument for input path."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'decoder setopt rejects None argument for input path'


def test_0138_python_api_decoder_setopt_rejects_none_argument() -> None:
    try:
        from libsixel_wheel import SIXEL_OPTFLAG_INPUT
        from libsixel_wheel import sixel_decoder_new
        from libsixel_wheel import sixel_decoder_setopt
        from libsixel_wheel import sixel_decoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    decoder = sixel_decoder_new()
    try:
        sixel_decoder_setopt(decoder, SIXEL_OPTFLAG_INPUT, None)
    except (RuntimeError, ValueError, TypeError):
        sixel_decoder_unref(decoder)
        print('decoder None argument rejection for setopt verified')
        return

    sixel_decoder_unref(decoder)
    raise SystemExit('decoder accepted None argument for input path')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0138_python_api_decoder_setopt_rejects_none_argument,
    ))
