#!/usr/bin/env python3
"""TAP test that packaged Python binding can encode sixel output."""

from __future__ import annotations

import os
from pathlib import Path

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encode output generated from packaged python binding'
ARTIFACT_LOCAL_DIR = os.path.expandvars("${ARTIFACT_LOCAL_DIR}")
os.makedirs(ARTIFACT_LOCAL_DIR, exist_ok=True)


def test_0003_python_bindings_encode_output() -> None:
    try:
        from libsixel_wheel import SIXEL_OPTFLAG_COLORS
        from libsixel_wheel import SIXEL_OPTFLAG_OUTPUT
        from libsixel_wheel.encoder import Encoder
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    source = Path(os.path.expandvars("${TOP_SRCDIR}/tests/data/inputs/snake_64.png"))
    output = Path(os.path.expandvars("${ARTIFACT_LOCAL_DIR}/python_bindings_smoke.six"))

    encoder = Encoder()
    encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(output))
    encoder.setopt(SIXEL_OPTFLAG_COLORS, "16")
    encoder.encode(str(source))

    payload = output.read_bytes()
    if not payload:
        raise SystemExit("encoded output is empty")
    if not payload.startswith(b"\x1bPq"):
        raise SystemExit("missing sixel DCS introducer")
    if not payload.rstrip(b"\r\n").endswith(b"\x1b\\"):
        raise SystemExit("missing sixel ST terminator")

    print("encode output generated from packaged python binding")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0003_python_bindings_encode_output))
