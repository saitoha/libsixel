#!/usr/bin/env python3
"""TAP test that encodes PNG via wheel (DCS/ST ok)."""

from __future__ import annotations

import os

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encodes PNG via wheel (DCS/ST ok)'
def test_0002_python_api_format_png() -> None:
    import pathlib

    try:
        from libsixel_wheel import SIXEL_OPTFLAG_INPUT
        from libsixel_wheel.encoder import Encoder, SIXEL_OPTFLAG_OUTPUT

        source = pathlib.Path(os.path.expandvars("${TOP_SRCDIR}/tests/data/inputs/snake_64.png"))
        target = pathlib.Path(os.path.expandvars("${ARTIFACT_LOCAL_DIR}/PNG.six"))

        encoder = Encoder()
        encoder.setopt(SIXEL_OPTFLAG_INPUT, str(source))
        encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(target))
        encoder.encode(str(source))

        if not target.exists() or target.stat().st_size == 0:
            raise SystemExit("missing or empty sixel output")

        data = target.read_bytes()
        if not data.startswith(b"\x1bPq"):
            raise SystemExit("missing sixel DCS introducer")
        if not data.rstrip(b"\r\n").endswith(b"\x1b\\"):
            raise SystemExit("missing sixel ST terminator")

        print(f"encoded {source.name} -> {target.name} ({len(data)} bytes)")
    except OSError as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0002_python_api_format_png))
