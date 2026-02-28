#!/usr/bin/env python3
"""TAP test that encoder setopt rejects None argument for color count."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encoder setopt rejects None argument for color count'


def test_0137_python_api_encoder_setopt_rejects_none_argument() -> None:
    try:
        from libsixel_wheel import SIXEL_OPTFLAG_COLORS
        from libsixel_wheel import sixel_encoder_new
        from libsixel_wheel import sixel_encoder_setopt
        from libsixel_wheel import sixel_encoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    encoder = sixel_encoder_new()
    try:
        sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_COLORS, None)
    except (RuntimeError, ValueError, TypeError):
        sixel_encoder_unref(encoder)
        print('encoder None argument rejection for setopt verified')
        return

    sixel_encoder_unref(encoder)
    raise SystemExit('encoder accepted None argument for color count')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0137_python_api_encoder_setopt_rejects_none_argument,
    ))
