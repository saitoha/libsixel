#!/usr/bin/env python3
"""TAP test that encoder encode_bytes rejects palette with non-integer component."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encoder encode_bytes rejects palette with non-integer component'


def test_0129_python_api_encoder_encode_bytes_rejects_palette_with_non_integer_component() -> None:
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
            bytes([255, 0, 0, 0, 0, 0, 0, 0]),
            1,
            1,
            SIXEL_PIXELFORMAT_RGB888,
            [255, 'x', 0],
        )
    except TypeError:
        sixel_encoder_unref(encoder)
        print('encoder non-integer palette-component rejection verified')
        return

    sixel_encoder_unref(encoder)
    raise SystemExit(
        'encoder encode_bytes accepted palette with non-integer component'
    )


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0129_python_api_encoder_encode_bytes_rejects_palette_with_non_integer_component,
    ))
