#!/usr/bin/env python3
"""TAP test for Decoder.decode()."""

from __future__ import annotations

import os

from _taptest import run_embedded_tap_test


DESCRIPTION = 'decoder decode writes png output via wheel'
def test_0021_python_api_decoder_decode() -> None:
    import pathlib

    try:
        from libsixel_wheel import SIXEL_OPTFLAG_INPUT, SIXEL_OPTFLAG_OUTPUT
        from libsixel_wheel.encoder import Encoder
        from libsixel_wheel.decoder import Decoder
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    source = pathlib.Path(os.path.expandvars("${TOP_SRCDIR}/tests/data/inputs/snake_64.png"))
    sixel_path = pathlib.Path(os.path.expandvars("${ARTIFACT_LOCAL_DIR}/decoder_input.six"))
    png_path = pathlib.Path(os.path.expandvars("${ARTIFACT_LOCAL_DIR}/decode.png"))

    encoder = Encoder()
    encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(sixel_path))
    encoder.encode(str(source))

    decoder = Decoder()
    decoder.setopt(SIXEL_OPTFLAG_INPUT, str(sixel_path))
    decoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(png_path))
    decoder.decode(str(sixel_path))
    decoder.decode()

    if not png_path.exists() or png_path.stat().st_size == 0:
        raise SystemExit("decoder output missing or empty")

    print("decoder decode verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0021_python_api_decoder_decode))
