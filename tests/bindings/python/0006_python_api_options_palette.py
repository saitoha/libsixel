#!/usr/bin/env python3
"""TAP test migrated from shell wrapper to Python-only execution."""

from __future__ import annotations

import os

from _taptest import run_embedded_tap_test


DESCRIPTION = 'palette/diffusion/quality options honor palette limit via wheel'
def test_0006_python_api_options_palette() -> None:
    import pathlib
    import re

    try:
        from libsixel_wheel import (
            SIXEL_OPTFLAG_BGCOLOR,
            SIXEL_OPTFLAG_COLORS,
            SIXEL_OPTFLAG_DIFFUSION,
            SIXEL_OPTFLAG_INPUT,
            SIXEL_OPTFLAG_OUTPUT,
            SIXEL_OPTFLAG_PALETTE_TYPE,
            SIXEL_OPTFLAG_QUALITY,
        )
        from libsixel_wheel.encoder import Encoder
    except OSError as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    source = pathlib.Path(os.path.expandvars("${TOP_SRCDIR}/tests/data/inputs/snake_64.png"))
    workdir = pathlib.Path(os.path.expandvars("${ARTIFACT_LOCAL_DIR}/palette"))
    workdir.mkdir(parents=True, exist_ok=True)
    output = workdir / "palette.six"

    encoder = Encoder()
    encoder.setopt(SIXEL_OPTFLAG_INPUT, str(source))
    encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(output))
    encoder.setopt(SIXEL_OPTFLAG_COLORS, "16")
    encoder.setopt(SIXEL_OPTFLAG_DIFFUSION, "atkinson")
    encoder.setopt(SIXEL_OPTFLAG_PALETTE_TYPE, "hls")
    encoder.setopt(SIXEL_OPTFLAG_QUALITY, "high")
    encoder.setopt(SIXEL_OPTFLAG_BGCOLOR, "#000000")
    encoder.encode(str(source))

    if not output.exists() or output.stat().st_size == 0:
        raise SystemExit("missing or empty sixel output")

    data = output.read_bytes()
    if not data.startswith(b"\x1bPq"):
        raise SystemExit("missing sixel DCS introducer")
    if not data.rstrip(b"\r\n").endswith(b"\x1b\\"):
        raise SystemExit("missing sixel ST terminator")

    palette = {int(entry) for entry in re.findall(rb"#(\d+)", data)}
    if not palette or len(palette) > 16:
        raise SystemExit("palette limit check failed")

    raster = re.search(rb'"(\d+);(\d+);(\d+);(\d+)', data)
    if raster:
        print(f"palette ok (<=16 entries), raster={raster.group(1).decode()}x{raster.group(2).decode()}")
    else:
        print("palette ok (<=16 entries)")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0006_python_api_options_palette))
