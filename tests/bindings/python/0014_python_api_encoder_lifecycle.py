#!/usr/bin/env python3
"""TAP test for Python encoder API lifecycle coverage."""

from __future__ import annotations

import os

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encoder methods work and reject calls after close via wheel'
def test_0014_python_api_encoder_lifecycle() -> None:
    import pathlib

    try:
        from libsixel_wheel import SIXEL_OPTFLAG_INPUT, SIXEL_OPTFLAG_OUTPUT
        from libsixel_wheel import SIXEL_PIXELFORMAT_RGB888
        from libsixel_wheel.encoder import Encoder
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    source = pathlib.Path(os.path.expandvars("${TOP_SRCDIR}/tests/data/inputs/snake_64.png"))
    workdir = pathlib.Path(os.path.expandvars("${ARTIFACT_LOCAL_DIR}/encoder_lifecycle"))
    workdir.mkdir(parents=True, exist_ok=True)
    output = workdir / "encoder.six"

    encoder = Encoder()
    encoder.setopt(SIXEL_OPTFLAG_INPUT, str(source))
    encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(output))
    encoder.encode(str(source))

    pixels = bytes([
        255, 0, 0,
        0, 255, 0,
        0, 0, 255,
        255, 255, 255,
    ])
    encoder.encode_bytes(pixels, 2, 2, SIXEL_PIXELFORMAT_RGB888, None)

    if not output.exists() or output.stat().st_size == 0:
        raise SystemExit("encoder output missing or empty")
    encoded = output.read_bytes()
    if not encoded.startswith(b"\x1bPq"):
        raise SystemExit("missing sixel DCS introducer")

    encoder.close()
    encoder.close()

    message_setopt = ""
    try:
        encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(output))
    except Exception as exc:  # noqa: BLE001
        if not isinstance(exc, RuntimeError):
            raise SystemExit(f"closed setopt: expected RuntimeError, got {type(exc).__name__}")
        message_setopt = str(exc)
    if "closed" not in message_setopt.lower():
        raise SystemExit("closed setopt: missing closed keyword")

    message_encode = ""
    try:
        encoder.encode(str(source))
    except Exception as exc:  # noqa: BLE001
        if not isinstance(exc, RuntimeError):
            raise SystemExit(f"closed encode: expected RuntimeError, got {type(exc).__name__}")
        message_encode = str(exc)
    if "closed" not in message_encode.lower():
        raise SystemExit("closed encode: missing closed keyword")

    message_encode_bytes = ""
    try:
        encoder.encode_bytes(pixels, 2, 2, SIXEL_PIXELFORMAT_RGB888, None)
    except Exception as exc:  # noqa: BLE001
        if not isinstance(exc, RuntimeError):
            raise SystemExit(
                f"closed encode_bytes: expected RuntimeError, got {type(exc).__name__}"
            )
        message_encode_bytes = str(exc)
    if "closed" not in message_encode_bytes.lower():
        raise SystemExit("closed encode_bytes: missing closed keyword")

    print("encoder lifecycle APIs verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0014_python_api_encoder_lifecycle))
