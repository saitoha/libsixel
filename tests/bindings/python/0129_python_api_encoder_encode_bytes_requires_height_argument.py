#!/usr/bin/env python3
"""TAP test that encode_bytes requires height argument."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encoder encode_bytes requires height argument'


def test_0132_python_api_encoder_encode_bytes_requires_height_argument() -> None:
    try:
        from libsixel_wheel import SIXEL_PIXELFORMAT_RGB888
        from libsixel_wheel import sixel_encoder_encode_bytes
        from libsixel_wheel import sixel_encoder_new
        from libsixel_wheel import sixel_encoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    encoder = sixel_encoder_new()
    try:
        sixel_encoder_encode_bytes(
            encoder,
            b'\x00\x00\x00',
            width=1,
            pixelformat=SIXEL_PIXELFORMAT_RGB888,
            palette=None,
        )
    except TypeError:
        sixel_encoder_unref(encoder)
        print('encoder encode_bytes missing height argument rejection verified')
        return

    sixel_encoder_unref(encoder)
    raise SystemExit('encoder encode_bytes accepted missing height argument')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0132_python_api_encoder_encode_bytes_requires_height_argument,
    ))
