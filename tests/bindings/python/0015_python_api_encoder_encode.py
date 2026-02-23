#!/usr/bin/env python3
"""TAP test for Encoder.encode()."""

from __future__ import annotations

import os

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encoder encode writes sixel output via wheel'
def test_0015_python_api_encoder_encode() -> None:
    import pathlib

    try:
        from libsixel_wheel import SIXEL_OPTFLAG_OUTPUT
        from libsixel_wheel.encoder import Encoder
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    source = pathlib.Path(os.path.expandvars("${TOP_SRCDIR}/tests/data/inputs/snake_64.png"))
    output = pathlib.Path(os.path.expandvars("${ARTIFACT_LOCAL_DIR}/encode.six"))

    encoder = Encoder()
    encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(output))
    encoder.encode(str(source))

    if not output.exists() or output.stat().st_size == 0:
        raise SystemExit("encoder output missing or empty")

    print("encoder encode verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0015_python_api_encoder_encode))
