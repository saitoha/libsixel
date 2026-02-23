#!/usr/bin/env python3
"""TAP test migrated from shell wrapper to Python-only execution."""

from __future__ import annotations

import os

from _taptest import run_embedded_tap_test


DESCRIPTION = 'closed encoder/decoder reject API calls via wheel'
def test_0014_python_api_closed_handle_errors() -> None:
    import pathlib

    try:
        from libsixel_wheel import (
            SIXEL_OPTFLAG_INPUT,
            SIXEL_OPTFLAG_OUTPUT,
        )
        from libsixel_wheel.decoder import Decoder
        from libsixel_wheel.encoder import Encoder
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    source = pathlib.Path(os.path.expandvars("${TOP_SRCDIR}/tests/data/inputs/snake_64.png"))
    workdir = pathlib.Path(os.path.expandvars("${ARTIFACT_LOCAL_DIR}/closed_handles"))
    workdir.mkdir(parents=True, exist_ok=True)
    output = workdir / "closed.six"

    encoder = Encoder()
    encoder.setopt(SIXEL_OPTFLAG_INPUT, str(source))
    encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(output))
    encoder.close()
    encoder.close()

    encoder_error = ""
    try:
        encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(output))
    except Exception as exc:  # noqa: BLE001
        if not isinstance(exc, RuntimeError):
            raise SystemExit(f"closed encoder: expected RuntimeError, got {type(exc).__name__}")
        encoder_error = str(exc)

    if "closed" not in encoder_error.lower():
        raise SystemExit("closed encoder: error message does not mention closed state")

    decoder = Decoder()
    decoder.setopt(SIXEL_OPTFLAG_INPUT, str(output))
    decoder.close()
    decoder.close()

    decoder_error = ""
    try:
        decoder.decode(str(output))
    except Exception as exc:  # noqa: BLE001
        if not isinstance(exc, RuntimeError):
            raise SystemExit(f"closed decoder: expected RuntimeError, got {type(exc).__name__}")
        decoder_error = str(exc)

    if "closed" not in decoder_error.lower():
        raise SystemExit("closed decoder: error message does not mention closed state")

    print("encoder/decoder close semantics verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0014_python_api_closed_handle_errors))
