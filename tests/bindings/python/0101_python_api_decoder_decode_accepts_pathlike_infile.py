#!/usr/bin/env python3
"""TAP test that decoder decode accepts pathlib.Path infile argument."""

from __future__ import annotations

import os

from _taptest import run_embedded_tap_test


DESCRIPTION = 'decoder decode accepts pathlib.Path infile argument'


def test_0104_python_api_decoder_decode_accepts_pathlike_infile() -> None:
    import pathlib

    try:
        from libsixel_wheel import sixel_decoder_decode
        from libsixel_wheel import SIXEL_OPTFLAG_OUTPUT
        from libsixel_wheel import sixel_decoder_new
        from libsixel_wheel import sixel_decoder_setopt
        from libsixel_wheel import sixel_decoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    infile = pathlib.Path(
        os.path.expandvars('${TOP_SRCDIR}/tests/data/inputs/snake_64.six')
    )
    output = pathlib.Path(
        os.path.expandvars(
            '${ARTIFACT_LOCAL_DIR}/decode_pathlike_infile_output.png'
        )
    )

    decoder = sixel_decoder_new()
    # Direct decoder output to a file so binary PNG bytes never leak
    # into the test runner's standard output stream.
    sixel_decoder_setopt(decoder, SIXEL_OPTFLAG_OUTPUT, str(output))
    sixel_decoder_decode(decoder, infile)
    sixel_decoder_unref(decoder)

    png_payload = output.read_bytes()
    if len(png_payload) == 0:
        raise SystemExit('decoder decode path-like infile did not write output')

    if not png_payload.startswith(b'\x89PNG\r\n\x1a\n'):
        raise SystemExit('decoder decode output is not a PNG stream')

    print('decoder decode path-like infile acceptance verified')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0104_python_api_decoder_decode_accepts_pathlike_infile,
    ))
