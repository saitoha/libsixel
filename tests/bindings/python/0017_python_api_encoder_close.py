#!/usr/bin/env python3
"""TAP test that encoder close is idempotent and enforces closed state."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encoder close is idempotent and enforces closed state'
def test_0017_python_api_encoder_close() -> None:
    try:
        from libsixel_wheel import SIXEL_OPTFLAG_OUTPUT
        from libsixel_wheel.encoder import Encoder
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    encoder = Encoder()
    encoder.close()
    encoder.close()

    message = ""
    try:
        encoder.setopt(SIXEL_OPTFLAG_OUTPUT, "ignored.six")
    except Exception as exc:  # noqa: BLE001
        if not isinstance(exc, RuntimeError):
            raise SystemExit(f"expected RuntimeError, got {type(exc).__name__}")
        message = str(exc)

    if "closed" not in message.lower():
        raise SystemExit("closed state was not reported")

    print("encoder close verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0017_python_api_encoder_close))
