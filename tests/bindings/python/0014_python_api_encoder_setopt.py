#!/usr/bin/env python3
"""TAP test for Encoder.setopt()."""

from __future__ import annotations

import os

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encoder setopt accepts input/output flags via wheel'
def test_0014_python_api_encoder_setopt() -> None:
    import pathlib

    try:
        from libsixel_wheel import SIXEL_OPTFLAG_INPUT, SIXEL_OPTFLAG_OUTPUT
        from libsixel_wheel.encoder import Encoder
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    source = pathlib.Path(os.path.expandvars("${TOP_SRCDIR}/tests/data/inputs/snake_64.png"))
    output = pathlib.Path(os.path.expandvars("${ARTIFACT_LOCAL_DIR}/setopt.six"))

    encoder = Encoder()
    encoder.setopt(SIXEL_OPTFLAG_INPUT, str(source))
    encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(output))

    print("encoder setopt verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0014_python_api_encoder_setopt))
