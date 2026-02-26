#!/usr/bin/env python3
"""TAP test that encoder encode_bytes rejects invalid pixelformat values."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encoder encode_bytes rejects invalid pixelformat values'
def test_0054_python_api_encoder_encode_bytes_invalid_pixelformat() -> None:
    try:
        from libsixel_wheel import sixel_encoder_encode_bytes
        from libsixel_wheel import sixel_encoder_new
        from libsixel_wheel import sixel_encoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    encoder = sixel_encoder_new()
    rejected = False
    try:
        sixel_encoder_encode_bytes(encoder, b"\x00\x00\x00", 1, 1, -1, None)
    except ValueError:
        rejected = True
    sixel_encoder_unref(encoder)

    if not rejected:
        raise SystemExit("encode_bytes accepted an invalid pixelformat")

    print("encode_bytes invalid-pixelformat path verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0054_python_api_encoder_encode_bytes_invalid_pixelformat))
