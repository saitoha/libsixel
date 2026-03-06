#!/usr/bin/env python3
"""TAP test that explicit resize dimensions survive decode via wheel."""

from __future__ import annotations

import os

from _taptest import run_embedded_tap_test


DESCRIPTION = 'explicit resize dimensions survive decode via wheel'
ARTIFACT_LOCAL_DIR = os.path.expandvars("${ARTIFACT_LOCAL_DIR}")
os.makedirs(ARTIFACT_LOCAL_DIR, exist_ok=True)

def test_0007_python_api_options_resize_fixed() -> None:
    import pathlib
    import re

    try:
        from libsixel_wheel import (
            SIXEL_OPTFLAG_HEIGHT,
            SIXEL_OPTFLAG_INPUT,
            SIXEL_OPTFLAG_OUTPUT,
            SIXEL_OPTFLAG_QUALITY,
            SIXEL_OPTFLAG_RESAMPLING,
            SIXEL_OPTFLAG_WIDTH,
        )
        from libsixel_wheel.decoder import Decoder
        from libsixel_wheel.encoder import Encoder
    except OSError as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    source = pathlib.Path(os.path.expandvars("${TOP_SRCDIR}/tests/data/inputs/snake_64.png"))
    output = pathlib.Path(os.path.expandvars("${ARTIFACT_LOCAL_DIR}/resize_fixed.six"))
    png = pathlib.Path(os.path.expandvars("${ARTIFACT_LOCAL_DIR}/resize_fixed.png"))

    encoder = Encoder()
    encoder.setopt(SIXEL_OPTFLAG_INPUT, str(source))
    encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(output))
    encoder.setopt(SIXEL_OPTFLAG_WIDTH, "64")
    encoder.setopt(SIXEL_OPTFLAG_HEIGHT, "32")
    encoder.setopt(SIXEL_OPTFLAG_RESAMPLING, "bilinear")
    encoder.setopt(SIXEL_OPTFLAG_QUALITY, "full")
    encoder.encode(str(source))

    if output.stat().st_size == 0:
        raise SystemExit("missing or empty sixel output")

    data = output.read_bytes()
    if not data.startswith(b"\x1bPq"):
        raise SystemExit("missing sixel DCS introducer")
    if not data.rstrip(b"\r\n").endswith(b"\x1b\\"):
        raise SystemExit("missing sixel ST terminator")

    raster = re.search(rb'"(\d+);(\d+);(\d+);(\d+)', data)

    decoder = Decoder()
    decoder.setopt(SIXEL_OPTFLAG_INPUT, str(output))
    decoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(png))
    decoder.decode(str(output))
    if png.stat().st_size == 0:
        raise SystemExit("decoder did not write output")

    header = png.read_bytes()
    if len(header) < 24 or header[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit("output is not a PNG")
    width = int.from_bytes(header[16:20], "big")
    height = int.from_bytes(header[20:24], "big")

    if (width, height) != (64, 32):
        raise SystemExit(f"expected 64x32, got {width}x{height}")
    if raster and (int(raster.group(1)) > 1 or int(raster.group(2)) > 1):
        if (int(raster.group(1)), int(raster.group(2))) != (64, 32):
            raise SystemExit("raster attribute mismatch")

    print("resize to 64x32 preserved in decode and raster")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0007_python_api_options_resize_fixed))
