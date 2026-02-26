#!/usr/bin/env python3
"""TAP test for memoryview buffer rejection in encoder.encode_bytes."""

from __future__ import annotations

import ctypes

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encoder encode_bytes rejects memoryview pixel buffer'


def test_0106_python_api_encoder_encode_bytes_rejects_memoryview() -> None:
    try:
        from libsixel_wheel import SIXEL_PIXELFORMAT_RGB888
        from libsixel_wheel import sixel_encoder_encode_bytes
        from libsixel_wheel import sixel_encoder_new
        from libsixel_wheel import sixel_encoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    pixels = memoryview(bytes([
        255, 0, 0,
        0, 255, 0,
        0, 0, 255,
        255, 255, 255,
    ]))

    encoder = sixel_encoder_new()
    try:
        sixel_encoder_encode_bytes(
            encoder,
            pixels,
            2,
            2,
            SIXEL_PIXELFORMAT_RGB888,
            None,
        )
    except (TypeError, ctypes.ArgumentError):
        sixel_encoder_unref(encoder)
        print('encoder encode_bytes memoryview buffer rejection verified')
        return

    sixel_encoder_unref(encoder)
    raise AssertionError('encoder encode_bytes accepted memoryview buffer input')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0106_python_api_encoder_encode_bytes_rejects_memoryview,
    ))
