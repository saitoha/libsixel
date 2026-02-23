#!/usr/bin/env python3
"""TAP test migrated from shell wrapper to Python-only execution."""

from __future__ import annotations

import os

from _taptest import run_embedded_tap_test


DESCRIPTION = 'unsupported format errors via wheel'
def test_0011_python_api_errors_unsupported_format() -> None:
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

    workdir = pathlib.Path(os.path.expandvars("${ARTIFACT_LOCAL_DIR}/unsupported"))
    workdir.mkdir(parents=True, exist_ok=True)
    text_file = workdir / "note.xxx"
    text_file.write_text("this is not an image")
    target = workdir / "note.six"

    message = ""
    try:
        encoder = Encoder()
        encoder.setopt(SIXEL_OPTFLAG_LOADERS, "builtin!")
        encoder.setopt(SIXEL_OPTFLAG_INPUT, str(text_file))
        encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(target))
        encoder.encode(str(text_file))
    except Exception as exc:  # noqa: BLE001
        if not isinstance(exc, RuntimeError):
            raise SystemExit(
                f"unsupported format: expected RuntimeError, got {type(exc).__name__}"
            )
        message = str(exc) or "<empty message>"

    if not message:
        raise SystemExit("unsupported format: expected exception but call succeeded")

    keywords = (
        "unsupported", "decode", "format", "cannot", "failed",
        "bad argument", "error", "stb_image"
    )
    if not any(keyword in message.lower() for keyword in keywords):
        raise SystemExit("unsupported format: missing expected keywords")

    print(f"unsupported format: RuntimeError ({message})")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0011_python_api_errors_unsupported_format))
