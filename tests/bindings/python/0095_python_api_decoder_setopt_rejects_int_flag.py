#!/usr/bin/env python3
"""TAP test that decoder setopt rejects integer option flag."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'decoder setopt rejects integer option flag'


def test_0095_python_api_decoder_setopt_rejects_int_flag() -> None:
    try:
        from libsixel_wheel import sixel_decoder_new
        from libsixel_wheel import sixel_decoder_setopt
        from libsixel_wheel import sixel_decoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    decoder = sixel_decoder_new()

    try:
        sixel_decoder_setopt(decoder, 111, 'dummy.png')
    except TypeError:
        sixel_decoder_unref(decoder)
        print('decoder integer option flag rejection verified')
        return

    sixel_decoder_unref(decoder)
    raise SystemExit('decoder accepted integer option flag')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0095_python_api_decoder_setopt_rejects_int_flag,
    ))
