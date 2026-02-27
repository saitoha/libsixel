#!/usr/bin/env python3
"""TAP test that raw decoder APIs create, configure, decode, and release."""

from __future__ import annotations

import os

from _taptest import run_embedded_tap_test


DESCRIPTION = 'raw decoder APIs create, configure, decode, and release'
def test_0029_python_api_raw_decoder_lifecycle() -> None:
    import pathlib

    try:
        from libsixel_wheel import SIXEL_OPTFLAG_INPUT
        from libsixel_wheel import SIXEL_OPTFLAG_OUTPUT
        from libsixel_wheel import sixel_decoder_decode
        from libsixel_wheel import sixel_decoder_new
        from libsixel_wheel import sixel_decoder_setopt
        from libsixel_wheel import sixel_decoder_unref
        from libsixel_wheel import sixel_encoder_encode
        from libsixel_wheel import sixel_encoder_new
        from libsixel_wheel import sixel_encoder_setopt
        from libsixel_wheel import sixel_encoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    source = pathlib.Path(os.path.expandvars("${TOP_SRCDIR}/tests/data/inputs/snake_64.png"))
    sixel_path = pathlib.Path(os.path.expandvars("${ARTIFACT_LOCAL_DIR}/raw_decoder_input.six"))
    png_path = pathlib.Path(os.path.expandvars("${ARTIFACT_LOCAL_DIR}/raw_decoder.png"))

    encoder = sixel_encoder_new()
    sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_OUTPUT, str(sixel_path))
    sixel_encoder_encode(encoder, str(source))
    sixel_encoder_unref(encoder)

    decoder = sixel_decoder_new()
    sixel_decoder_setopt(decoder, SIXEL_OPTFLAG_INPUT, str(sixel_path))
    sixel_decoder_setopt(decoder, SIXEL_OPTFLAG_OUTPUT, str(png_path))
    sixel_decoder_decode(decoder, str(sixel_path))
    sixel_decoder_unref(decoder)

    if png_path.stat().st_size == 0:
        raise SystemExit("raw decoder output missing or empty")

    print("raw decoder APIs verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0029_python_api_raw_decoder_lifecycle))
