#!/usr/bin/env python3
"""TAP test migrated from shell wrapper to Python-only execution."""

from __future__ import annotations

import os

from _taptest import run_embedded_tap_test


DESCRIPTION = 'missing input path errors via wheel'
ARGV = [os.path.expandvars("${ARTIFACT_LOCAL_DIR}/missing")]
def test_0009_python_api_errors_missing_path() -> None:
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
    missing = workdir / "does-not-exist.png"
    target = workdir / "missing.six"

    message = ""
    try:
        encoder = Encoder()
        encoder.setopt(SIXEL_OPTFLAG_LOADERS, "builtin!")
        encoder.setopt(SIXEL_OPTFLAG_INPUT, str(missing))
        encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(target))
        encoder.encode(str(missing))
    except Exception as exc:  # noqa: BLE001
        if not isinstance(exc, RuntimeError):
            raise SystemExit(f"missing input: expected RuntimeError, got {type(exc).__name__}")
        message = str(exc) or "<empty message>"

    if not message:
        raise SystemExit("missing input: expected exception but call succeeded")

    keywords = (
        "no such file", "cannot", "failed", "open",
        "not found", "bad argument", "does not exist"
    )
    if not any(keyword in message.lower() for keyword in keywords):
        raise SystemExit("missing input: missing expected keywords")

    print(f"missing input: RuntimeError ({message})")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, ARGV, test_0009_python_api_errors_missing_path))
