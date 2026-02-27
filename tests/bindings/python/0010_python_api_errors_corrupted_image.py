#!/usr/bin/env python3
"""TAP test that corrupted image errors via wheel."""

from __future__ import annotations

import os

from _taptest import run_embedded_tap_test


DESCRIPTION = 'corrupted image errors via wheel'
def test_0010_python_api_errors_corrupted_image() -> None:
    import pathlib

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

    artifact_dir = pathlib.Path(os.path.expandvars("${ARTIFACT_LOCAL_DIR}"))
    broken_bmp = artifact_dir / "broken.bmp"
    broken_bmp.write_bytes(b"BM\x00\x00")
    target = artifact_dir / "broken.six"

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

    keywords = ("corrupt", "invalid", "decode", "failed", "error")
    if not any(keyword in message.lower() for keyword in keywords):
        raise SystemExit("corrupted bmp: missing expected keywords")

    print(f"corrupted bmp: RuntimeError ({message})")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0010_python_api_errors_corrupted_image))
