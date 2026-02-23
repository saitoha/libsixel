#!/usr/bin/env python3
"""TAP test for Python decoder API lifecycle coverage."""

from __future__ import annotations

import os

from _taptest import run_embedded_tap_test


DESCRIPTION = 'decoder methods work and reject calls after close via wheel'
def test_0015_python_api_decoder_lifecycle() -> None:
    import pathlib

    try:
        from libsixel_wheel import SIXEL_OPTFLAG_INPUT, SIXEL_OPTFLAG_OUTPUT
        from libsixel_wheel.encoder import Encoder
        from libsixel_wheel.decoder import Decoder
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    source = pathlib.Path(os.path.expandvars("${TOP_SRCDIR}/tests/data/inputs/snake_64.png"))
    workdir = pathlib.Path(os.path.expandvars("${ARTIFACT_LOCAL_DIR}/decoder_lifecycle"))
    workdir.mkdir(parents=True, exist_ok=True)
    sixel_path = workdir / "decoder.six"
    png_path = workdir / "decoder.png"

    encoder = Encoder()
    encoder.setopt(SIXEL_OPTFLAG_INPUT, str(source))
    encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(sixel_path))
    encoder.encode(str(source))
    encoder.close()

    decoder = Decoder()
    decoder.setopt(SIXEL_OPTFLAG_INPUT, str(sixel_path))
    decoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(png_path))
    decoder.decode(str(sixel_path))
    decoder.decode()

    if not png_path.exists() or png_path.stat().st_size == 0:
        raise SystemExit("decoder output missing or empty")
    header = png_path.read_bytes()
    if len(header) < 24 or header[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit("decoder output is not PNG")

    decoder.close()
    decoder.close()

    message_setopt = ""
    try:
        decoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(png_path))
    except Exception as exc:  # noqa: BLE001
        if not isinstance(exc, RuntimeError):
            raise SystemExit(f"closed setopt: expected RuntimeError, got {type(exc).__name__}")
        message_setopt = str(exc)
    if "closed" not in message_setopt.lower():
        raise SystemExit("closed setopt: missing closed keyword")

    message_decode = ""
    try:
        decoder.decode(str(sixel_path))
    except Exception as exc:  # noqa: BLE001
        if not isinstance(exc, RuntimeError):
            raise SystemExit(f"closed decode: expected RuntimeError, got {type(exc).__name__}")
        message_decode = str(exc)
    if "closed" not in message_decode.lower():
        raise SystemExit("closed decode: missing closed keyword")

    print("decoder lifecycle APIs verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0015_python_api_decoder_lifecycle))
