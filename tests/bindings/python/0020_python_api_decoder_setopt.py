#!/usr/bin/env python3
"""TAP test for Decoder.setopt()."""

from __future__ import annotations

import os

from _taptest import run_embedded_tap_test


DESCRIPTION = 'decoder setopt accepts input/output flags via wheel'
def test_0020_python_api_decoder_setopt() -> None:
    import pathlib

    try:
        from libsixel_wheel import SIXEL_OPTFLAG_INPUT, SIXEL_OPTFLAG_OUTPUT
        from libsixel_wheel.decoder import Decoder
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    source = pathlib.Path(os.path.expandvars("${TOP_SRCDIR}/tests/data/inputs/snake_64.six"))
    output = pathlib.Path(os.path.expandvars("${ARTIFACT_LOCAL_DIR}/decode_setopt.png"))

    decoder = Decoder()
    decoder.setopt(SIXEL_OPTFLAG_INPUT, str(source))
    decoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(output))

    print("decoder setopt verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0020_python_api_decoder_setopt))
