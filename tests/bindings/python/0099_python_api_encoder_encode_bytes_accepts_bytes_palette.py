#!/usr/bin/env python3
"""TAP test that encoder encode_bytes accepts bytes palette input."""

from __future__ import annotations

import os

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encoder encode_bytes accepts bytes palette input'
ARTIFACT_LOCAL_DIR = os.path.expandvars("${ARTIFACT_LOCAL_DIR}")
os.makedirs(ARTIFACT_LOCAL_DIR, exist_ok=True)



def test_0099_python_api_encoder_encode_bytes_accepts_bytes_palette() -> None:
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

    sixel_payload = output.read_bytes()
    if len(sixel_payload) == 0:
        raise SystemExit('encoder output missing for bytes palette')

    if not (sixel_payload.startswith(b'\x1bP') and
            sixel_payload.endswith(b'\x1b\\')):
        raise SystemExit('encoder output is not a valid sixel envelope')

    print('encoder encode_bytes bytes palette acceptance verified')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0099_python_api_encoder_encode_bytes_accepts_bytes_palette,
    ))
