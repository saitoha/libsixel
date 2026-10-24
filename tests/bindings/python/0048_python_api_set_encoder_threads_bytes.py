#!/usr/bin/env python3
"""TAP test that set_encoder_threads accepts byte inputs and rejects invalid bytes."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'set_encoder_threads accepts byte inputs and rejects invalid bytes'
def test_0048_python_api_set_encoder_threads_bytes() -> None:
    try:
        from libsixel_wheel import sixel_set_encoder_threads
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    sixel_set_encoder_threads(b"2")
    sixel_set_encoder_threads(b"auto")

    rejected = False
    try:
        sixel_set_encoder_threads(b"bad")
    except ValueError:
        rejected = True

    if not rejected:
        raise SystemExit("set_encoder_threads accepted invalid byte input")

    print("set_encoder_threads byte input paths verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0048_python_api_set_encoder_threads_bytes))
