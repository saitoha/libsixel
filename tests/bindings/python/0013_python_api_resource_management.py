#!/usr/bin/env python3
"""TAP test that small roundtrip via wheel closes file resources promptly."""

from __future__ import annotations

import os

from _taptest import run_embedded_tap_test


DESCRIPTION = 'small roundtrip via wheel closes file resources promptly'
def test_0013_python_api_resource_management() -> None:
    import pathlib
    import warnings

    try:
        from libsixel_wheel import (
            SIXEL_OPTFLAG_INPUT,
            SIXEL_OPTFLAG_OUTPUT,
            SIXEL_OPTFLAG_WIDTH,
            SIXEL_OPTFLAG_HEIGHT,
        )
        from libsixel_wheel.encoder import Encoder
        from libsixel_wheel.decoder import Decoder
    except OSError as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    source = pathlib.Path(os.path.expandvars("${TOP_SRCDIR}/tests/data/inputs/snake_64.png"))
    artifact_dir = pathlib.Path(os.path.expandvars("${ARTIFACT_LOCAL_DIR}"))
    sixel_path = artifact_dir / "resource_roundtrip.six"
    decoded_png = artifact_dir / "resource_roundtrip.png"

    with warnings.catch_warnings():
        warnings.simplefilter("error", ResourceWarning)

        with Encoder() as encoder:
            encoder.setopt(SIXEL_OPTFLAG_INPUT, str(source))
            encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(sixel_path))
            encoder.setopt(SIXEL_OPTFLAG_WIDTH, "96")
            encoder.setopt(SIXEL_OPTFLAG_HEIGHT, "72")
            encoder.encode(str(source))

        if not sixel_path.exists() or sixel_path.stat().st_size == 0:
            raise SystemExit("encoder output missing or empty")
        size = sixel_path.stat().st_size

        with Decoder() as decoder:
            decoder.setopt(SIXEL_OPTFLAG_INPUT, str(sixel_path))
            decoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(decoded_png))
            decoder.decode(str(sixel_path))

        if not decoded_png.exists() or decoded_png.stat().st_size == 0:
            raise SystemExit("decoder output missing or empty")
        header = decoded_png.read_bytes()[:24]
        if len(header) < 24:
            raise SystemExit("decoded PNG header too small")
        width = int.from_bytes(header[16:20], "big")
        height = int.from_bytes(header[20:24], "big")

    try:
        sixel_path.unlink()
    except FileNotFoundError:
        pass

    try:
        decoded_png.unlink()
    except FileNotFoundError:
        pass

    if sixel_path.exists() or decoded_png.exists():
        raise SystemExit("output files persist after deletion")

    print(f"encoded {size} bytes, decoded {width}x{height}, resources cleaned")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0013_python_api_resource_management))
