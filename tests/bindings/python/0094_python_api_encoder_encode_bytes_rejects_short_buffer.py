#!/usr/bin/env python3
"""TAP test that encoder encode_bytes rejects too-short pixel buffer."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encoder encode_bytes rejects too-short pixel buffer'


def test_0097_python_api_encoder_encode_bytes_rejects_short_buffer() -> None:
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
            bytes([255, 0, 0]),
            2,
            2,
            SIXEL_PIXELFORMAT_RGB888,
            None,
        )
    except (ValueError, AttributeError):
        sixel_encoder_unref(encoder)
        print('encoder short buffer rejection verified')
        return

    sixel_encoder_unref(encoder)
    raise SystemExit('encoder encode_bytes accepted a too-short pixel buffer')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0097_python_api_encoder_encode_bytes_rejects_short_buffer,
    ))
