#!/usr/bin/env python3
"""TAP test that raw decoder decode accepts explicit infile argument."""

from __future__ import annotations

import os

from _taptest import run_embedded_tap_test


DESCRIPTION = 'raw decoder decode accepts explicit infile argument'
ARTIFACT_LOCAL_DIR = os.path.expandvars("${ARTIFACT_LOCAL_DIR}")
os.makedirs(ARTIFACT_LOCAL_DIR, exist_ok=True)



def test_0073_python_api_decoder_decode_infile_argument() -> None:
    import pathlib

    try:
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

    source = pathlib.Path(os.path.expandvars('${TOP_SRCDIR}/tests/data/inputs/snake_64.png'))
    sixel_path = pathlib.Path(os.path.expandvars('${ARTIFACT_LOCAL_DIR}/decode_infile_arg_input.six'))
    png_path = pathlib.Path(os.path.expandvars('${ARTIFACT_LOCAL_DIR}/decode_infile_arg_output.png'))

    encoder = sixel_encoder_new()
    sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_OUTPUT, str(sixel_path))
    sixel_encoder_encode(encoder, str(source))
    sixel_encoder_unref(encoder)

    decoder = sixel_decoder_new()
    sixel_decoder_setopt(decoder, SIXEL_OPTFLAG_OUTPUT, str(png_path))
    sixel_decoder_decode(decoder, str(sixel_path))
    sixel_decoder_unref(decoder)

    if png_path.stat().st_size == 0:
        raise SystemExit('decoder decode(infile=...) did not produce output')

    print('decoder decode infile-argument path verified')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0073_python_api_decoder_decode_infile_argument,
    ))
