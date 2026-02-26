#!/usr/bin/env python3
"""TAP test for bytes flag acceptance in decoder setopt."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'decoder setopt accepts bytes option flag'


def test_0096_python_api_decoder_setopt_accepts_bytes_flag() -> None:
    try:
        from libsixel_wheel import sixel_decoder_new
        from libsixel_wheel import sixel_decoder_setopt
        from libsixel_wheel import sixel_decoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    decoder = sixel_decoder_new()
    sixel_decoder_setopt(decoder, b'o', 'dummy.png')
    sixel_decoder_unref(decoder)

    print('decoder bytes option flag acceptance verified')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0096_python_api_decoder_setopt_accepts_bytes_flag,
    ))
