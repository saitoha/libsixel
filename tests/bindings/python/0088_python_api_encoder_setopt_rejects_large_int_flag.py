#!/usr/bin/env python3
"""TAP test for oversized integer flag rejection in encoder setopt."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encoder setopt rejects oversized integer flag values'


def test_0088_python_api_encoder_setopt_rejects_large_int_flag() -> None:
    try:
        from libsixel_wheel import sixel_encoder_new
        from libsixel_wheel import sixel_encoder_setopt
        from libsixel_wheel import sixel_encoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    encoder = sixel_encoder_new()

    try:
        sixel_encoder_setopt(encoder, 0x110000, '16')
    except ValueError:
        sixel_encoder_unref(encoder)
        print('encoder oversized integer flag rejection verified')
        return

    sixel_encoder_unref(encoder)
    raise AssertionError('encoder accepted oversized integer option flag')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0088_python_api_encoder_setopt_rejects_large_int_flag,
    ))
