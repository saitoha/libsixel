#!/usr/bin/env python3
"""TAP test that encoder __exit__ closes instance via wheel."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encoder __exit__ closes instance via wheel'
def test_0019_python_api_encoder_exit() -> None:
    try:
        from libsixel_wheel import SIXEL_OPTFLAG_OUTPUT
        from libsixel_wheel.encoder import Encoder
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    encoder = Encoder()
    encoder.__exit__(None, None, None)

    message = ""
    try:
        encoder.setopt(SIXEL_OPTFLAG_OUTPUT, "ignored.six")
    except Exception as exc:  # noqa: BLE001
        if not isinstance(exc, RuntimeError):
            raise SystemExit(f"expected RuntimeError, got {type(exc).__name__}")
        message = str(exc)

    if "closed" not in message.lower():
        raise SystemExit("__exit__ did not close encoder")

    print("encoder __exit__ verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0019_python_api_encoder_exit))
