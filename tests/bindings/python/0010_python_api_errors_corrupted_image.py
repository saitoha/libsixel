#!/usr/bin/env python3
"""TAP test migrated from shell wrapper to Python-only execution."""

from __future__ import annotations

import os

from _taptest import run_embedded_tap_test


DESCRIPTION = 'corrupted image errors via wheel'
ARGV = [os.path.expandvars("${ARTIFACT_LOCAL_DIR}/corrupt")]
def test_0010_python_api_errors_corrupted_image() -> None:
    import pathlib
    import sys

    try:
        from libsixel_wheel import (
            SIXEL_OPTFLAG_INPUT,
            SIXEL_OPTFLAG_LOADERS,
            SIXEL_OPTFLAG_OUTPUT,
        )
        from libsixel_wheel.encoder import Encoder
    except OSError as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    workdir = pathlib.Path(sys.argv[1])
    workdir.mkdir(parents=True, exist_ok=True)
    broken_bmp = workdir / "broken.bmp"
    broken_bmp.write_bytes(b"BM\x00\x00")
    target = workdir / "broken.six"

    message = ""
    try:
        encoder = Encoder()
        encoder.setopt(SIXEL_OPTFLAG_LOADERS, "builtin!")
        encoder.setopt(SIXEL_OPTFLAG_INPUT, str(broken_bmp))
        encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(target))
        encoder.encode(str(broken_bmp))
    except Exception as exc:  # noqa: BLE001
        if not isinstance(exc, RuntimeError):
            raise SystemExit(f"corrupted bmp: expected RuntimeError, got {type(exc).__name__}")
        message = str(exc) or "<empty message>"

    if not message:
        raise SystemExit("corrupted bmp: expected exception but call succeeded")

    print(f"corrupted bmp: RuntimeError ({message})")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, ARGV, test_0010_python_api_errors_corrupted_image))
