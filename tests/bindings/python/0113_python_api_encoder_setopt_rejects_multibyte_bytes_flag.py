#!/usr/bin/env python3
"""TAP test for multi-byte bytes flag rejection in encoder setopt."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encoder setopt rejects multi-byte bytes option flag input'


def test_0113_python_api_encoder_setopt_rejects_multibyte_bytes_flag() -> None:
    try:
        from libsixel_wheel import sixel_encoder_new
        from libsixel_wheel import sixel_encoder_setopt
        from libsixel_wheel import sixel_encoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    encoder = sixel_encoder_new()

    try:
        sixel_encoder_setopt(encoder, b'pp', 256)
    except RuntimeError:
        sixel_encoder_unref(encoder)
        print('encoder multi-byte bytes option flag rejection verified')
        return

    sixel_encoder_unref(encoder)
    raise AssertionError('encoder accepted multi-byte bytes option flag input')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0113_python_api_encoder_setopt_rejects_multibyte_bytes_flag,
    ))
