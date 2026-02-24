#!/usr/bin/env python3
"""TAP test for character flag path in sixel_decoder_setopt."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'decoder setopt accepts character flag values'
def test_0053_python_api_decoder_setopt_char_flag() -> None:
    try:
        from libsixel_wheel import SIXEL_OPTFLAG_OUTPUT
        from libsixel_wheel import sixel_decoder_new
        from libsixel_wheel import sixel_decoder_setopt
        from libsixel_wheel import sixel_decoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    decoder = sixel_decoder_new()
    sixel_decoder_setopt(decoder, SIXEL_OPTFLAG_OUTPUT, "dummy.png")
    sixel_decoder_unref(decoder)

    print("decoder setopt character-flag path verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0053_python_api_decoder_setopt_char_flag))
