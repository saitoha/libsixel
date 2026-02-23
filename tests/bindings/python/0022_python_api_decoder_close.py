#!/usr/bin/env python3
"""TAP test for Decoder.close()."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'decoder close is idempotent and enforces closed state'
def test_0022_python_api_decoder_close() -> None:
    try:
        from libsixel_wheel import SIXEL_OPTFLAG_OUTPUT
        from libsixel_wheel.decoder import Decoder
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    decoder = Decoder()
    decoder.close()
    decoder.close()

    message = ""
    try:
        decoder.setopt(SIXEL_OPTFLAG_OUTPUT, "ignored.png")
    except Exception as exc:  # noqa: BLE001
        if not isinstance(exc, RuntimeError):
            raise SystemExit(f"expected RuntimeError, got {type(exc).__name__}")
        message = str(exc)

    if "closed" not in message.lower():
        raise SystemExit("closed state was not reported")

    print("decoder close verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0022_python_api_decoder_close))
