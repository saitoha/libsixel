#!/usr/bin/env python3
"""TAP test that encoder encode_bytes writes sixel output via wheel."""

from __future__ import annotations

import os

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encoder encode_bytes writes sixel output via wheel'
ARTIFACT_LOCAL_DIR = os.path.expandvars("${ARTIFACT_LOCAL_DIR}")
os.makedirs(ARTIFACT_LOCAL_DIR, exist_ok=True)

def test_0016_python_api_encoder_encode_bytes() -> None:
    import pathlib

    try:
        from libsixel_wheel import SIXEL_OPTFLAG_OUTPUT, SIXEL_PIXELFORMAT_RGB888
        from libsixel_wheel.encoder import Encoder
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    output = pathlib.Path(os.path.expandvars("${ARTIFACT_LOCAL_DIR}/encode_bytes.six"))
    pixels = bytes([255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255])

    encoder = Encoder()
    encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(output))
    encoder.encode_bytes(pixels, 2, 2, SIXEL_PIXELFORMAT_RGB888, None)

    if output.stat().st_size == 0:
        raise SystemExit("encoder output missing or empty")

    print("encoder encode_bytes verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0016_python_api_encoder_encode_bytes))
