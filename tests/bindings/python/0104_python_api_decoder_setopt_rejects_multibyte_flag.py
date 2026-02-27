#!/usr/bin/env python3
"""TAP test that decoder setopt rejects multi-byte bytes option flag."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'decoder setopt rejects multi-byte bytes option flag'


def test_0107_python_api_decoder_setopt_rejects_multibyte_flag() -> None:
    try:
        from libsixel_wheel import sixel_decoder_new
        from libsixel_wheel import sixel_decoder_setopt
        from libsixel_wheel import sixel_decoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    decoder = sixel_decoder_new()

    try:
        sixel_decoder_setopt(decoder, b'xx', 'dummy.png')
    except TypeError:
        sixel_decoder_unref(decoder)
        print('decoder multi-byte bytes option flag rejection verified')
        return

    sixel_decoder_unref(decoder)
    raise SystemExit('decoder accepted multi-byte bytes option flag')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0107_python_api_decoder_setopt_rejects_multibyte_flag,
    ))
