#!/usr/bin/env python3
"""TAP test that encoder setopt rejects multi-character option flags."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encoder setopt rejects multi-character option flags'


def test_0081_python_api_encoder_setopt_invalid_flag_length() -> None:
    try:
        from libsixel_wheel import sixel_encoder_new
        from libsixel_wheel import sixel_encoder_setopt
        from libsixel_wheel import sixel_encoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    encoder = sixel_encoder_new()

    try:
        sixel_encoder_setopt(encoder, 'xy', '16')
    except RuntimeError:
        sixel_encoder_unref(encoder)
        print('encoder multi-character flag rejection verified')
        return

    sixel_encoder_unref(encoder)
    raise AssertionError('encoder accepted multi-character option flag')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0081_python_api_encoder_setopt_invalid_flag_length,
    ))
