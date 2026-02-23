#!/usr/bin/env python3
"""TAP test for Decoder.__enter__()."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'decoder __enter__ returns same instance via wheel'
def test_0023_python_api_decoder_enter() -> None:
    try:
        from libsixel_wheel.decoder import Decoder
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    decoder = Decoder()
    entered = decoder.__enter__()
    if entered is not decoder:
        raise SystemExit("__enter__ did not return self")
    decoder.close()

    print("decoder __enter__ verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0023_python_api_decoder_enter))
