#!/usr/bin/env python3
"""TAP test migrated from shell wrapper to Python-only execution."""

from __future__ import annotations

import os
import sys

from _taptest import run_embedded_tap_test


DESCRIPTION = 'invalid option value errors via wheel'
def test_0012_python_api_errors_invalid_option() -> None:
    import pathlib
    import re

    try:
        from libsixel_wheel import (
            SIXEL_OPTFLAG_COLORS,
            SIXEL_OPTFLAG_INPUT,
            SIXEL_OPTFLAG_LOADERS,
            SIXEL_OPTFLAG_OUTPUT,
            SIXEL_OPTFLAG_START_FRAME,
        )
        from libsixel_wheel.encoder import Encoder
    except OSError as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    source = pathlib.Path(os.path.expandvars("${TOP_SRCDIR}/tests/data/inputs/snake_64.png"))
    workdir = pathlib.Path(os.path.expandvars("${ARTIFACT_LOCAL_DIR}/invalid_option"))
    header = pathlib.Path(os.path.expandvars("${TOP_SRCDIR}/include/sixel.h.in"))
    workdir.mkdir(parents=True, exist_ok=True)
    target = workdir / "invalid-option.six"

    module = sys.modules["libsixel_wheel"]
    expected = set(
        re.findall(
            r"#define\s+(SIXEL_OPTFLAG_[A-Z0-9_]+)\s+\(",
            header.read_text(encoding="utf-8"),
        )
    )
    missing = sorted(name for name in expected if not hasattr(module, name))
    if missing:
        raise SystemExit(
            "missing wheel optflag constants: " + ", ".join(missing)
        )

    message = ""
    try:
        encoder = Encoder()
        encoder.setopt(SIXEL_OPTFLAG_LOADERS, "builtin!")
        encoder.setopt(SIXEL_OPTFLAG_START_FRAME, "0")
        encoder.setopt(SIXEL_OPTFLAG_COLORS, "-1")
        encoder.setopt(SIXEL_OPTFLAG_INPUT, str(source))
        encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(target))
        encoder.encode(str(source))
    except Exception as exc:  # noqa: BLE001
        if not isinstance(exc, RuntimeError):
            raise SystemExit(
                f"invalid option value: expected RuntimeError, got {type(exc).__name__}"
            )
        message = str(exc) or "<empty message>"

    if not message:
        raise SystemExit("invalid option value: expected exception but call succeeded")

    keywords = (
        "invalid", "colors", "range", "parameter", "option",
        "bad argument", "value", "must"
    )
    if not any(keyword in message.lower() for keyword in keywords):
        raise SystemExit("invalid option value: missing expected keywords")

    print(f"invalid option value: RuntimeError ({message})")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0012_python_api_errors_invalid_option))
