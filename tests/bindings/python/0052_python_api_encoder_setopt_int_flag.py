#!/usr/bin/env python3
"""TAP test that encoder setopt accepts integer flag values."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encoder setopt accepts integer flag values'
def test_0052_python_api_encoder_setopt_int_flag() -> None:
    try:
        from libsixel_wheel import SIXEL_OPTFLAG_COLORS
        from libsixel_wheel import sixel_encoder_new
        from libsixel_wheel import sixel_encoder_setopt
        from libsixel_wheel import sixel_encoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    encoder = sixel_encoder_new()
    # Pass a numeric flag value to verify the integer-flag call path.
    sixel_encoder_setopt(encoder, ord(SIXEL_OPTFLAG_COLORS), 16)
    sixel_encoder_unref(encoder)

    print("encoder setopt integer-flag path verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0052_python_api_encoder_setopt_int_flag))
