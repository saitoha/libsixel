#!/usr/bin/env python3
"""TAP test for negative integer flag rejection in encoder setopt."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encoder setopt rejects negative integer flag values'


def test_0084_python_api_encoder_setopt_rejects_negative_int_flag() -> None:
    try:
        from libsixel_wheel import sixel_encoder_new
        from libsixel_wheel import sixel_encoder_setopt
        from libsixel_wheel import sixel_encoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    encoder = sixel_encoder_new()

    try:
        sixel_encoder_setopt(encoder, -1, '16')
    except ValueError:
        sixel_encoder_unref(encoder)
        print('encoder negative integer flag rejection verified')
        return

    sixel_encoder_unref(encoder)
    raise AssertionError('encoder accepted negative integer option flag')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0084_python_api_encoder_setopt_rejects_negative_int_flag,
    ))
