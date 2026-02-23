#!/usr/bin/env python3
"""TAP test for Encoder.__enter__()."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encoder __enter__ returns same instance via wheel'
def test_0018_python_api_encoder_enter() -> None:
    try:
        from libsixel_wheel.encoder import Encoder
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    encoder = Encoder()
    entered = encoder.__enter__()
    if entered is not encoder:
        raise SystemExit("__enter__ did not return self")
    encoder.close()

    print("encoder __enter__ verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0018_python_api_encoder_enter))
