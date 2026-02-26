#!/usr/bin/env python3
"""TAP test that encoder setopt rejects empty string option flag."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encoder setopt rejects empty string option flag'


def test_0089_python_api_encoder_setopt_rejects_empty_flag() -> None:
    try:
        from libsixel_wheel import sixel_encoder_new
        from libsixel_wheel import sixel_encoder_setopt
        from libsixel_wheel import sixel_encoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    encoder = sixel_encoder_new()

    try:
        sixel_encoder_setopt(encoder, '', '16')
    except RuntimeError:
        sixel_encoder_unref(encoder)
        print('encoder empty option flag rejection verified')
        return

    sixel_encoder_unref(encoder)
    raise AssertionError('encoder accepted empty option flag')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0089_python_api_encoder_setopt_rejects_empty_flag,
    ))
