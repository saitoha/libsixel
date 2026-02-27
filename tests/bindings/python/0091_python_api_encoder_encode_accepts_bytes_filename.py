#!/usr/bin/env python3
"""TAP test that encoder encode accepts bytes filename input."""

from __future__ import annotations

import os

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encoder encode accepts bytes filename input'


def test_0091_python_api_encoder_encode_accepts_bytes_filename() -> None:
    import pathlib

    try:
        from libsixel_wheel import SIXEL_OPTFLAG_OUTPUT
        from libsixel_wheel import sixel_encoder_encode
        from libsixel_wheel import sixel_encoder_new
        from libsixel_wheel import sixel_encoder_setopt
        from libsixel_wheel import sixel_encoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    source = pathlib.Path(
        os.path.expandvars("${TOP_SRCDIR}/tests/data/inputs/snake_64.png")
    )
    output = pathlib.Path(
        os.path.expandvars("${ARTIFACT_LOCAL_DIR}/encode_bytes_filename.six")
    )

    encoder = sixel_encoder_new()
    sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_OUTPUT, str(output))
    sixel_encoder_encode(encoder, bytes(source))
    sixel_encoder_unref(encoder)

    if output.stat().st_size == 0:
        raise SystemExit('encoder output missing for bytes filename input')

    print('encoder bytes filename acceptance verified')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0091_python_api_encoder_encode_accepts_bytes_filename,
    ))
