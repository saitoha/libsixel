#!/usr/bin/env python3
"""TAP test for bytes palette acceptance in encoder.encode_bytes."""

from __future__ import annotations

import os

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encoder encode_bytes accepts bytes palette input'


def test_0103_python_api_encoder_encode_bytes_accepts_bytes_palette() -> None:
    import pathlib

    try:
        from libsixel_wheel import SIXEL_OPTFLAG_OUTPUT
        from libsixel_wheel import SIXEL_PIXELFORMAT_PAL8
        from libsixel_wheel import sixel_encoder_encode_bytes
        from libsixel_wheel import sixel_encoder_new
        from libsixel_wheel import sixel_encoder_setopt
        from libsixel_wheel import sixel_encoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    output = pathlib.Path(
        os.path.expandvars('${ARTIFACT_LOCAL_DIR}/encode_bytes_bytes_palette.six')
    )
    pixels = bytes([0, 1, 2, 3, 0, 1, 2, 3])
    palette = bytes([
        255, 0, 0,
        0, 255, 0,
        0, 0, 255,
        255, 255, 255,
    ])

    encoder = sixel_encoder_new()
    sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_OUTPUT, str(output))
    sixel_encoder_encode_bytes(
        encoder,
        pixels,
        4,
        2,
        SIXEL_PIXELFORMAT_PAL8,
        palette,
    )
    sixel_encoder_unref(encoder)

    if not output.exists() or output.stat().st_size == 0:
        raise AssertionError('encoder output missing for bytes palette')

    print('encoder encode_bytes bytes palette acceptance verified')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0103_python_api_encoder_encode_bytes_accepts_bytes_palette,
    ))
