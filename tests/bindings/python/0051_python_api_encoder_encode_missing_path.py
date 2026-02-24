#!/usr/bin/env python3
"""TAP test for path validation in sixel_encoder_encode."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encoder encode rejects missing input path'
def test_0051_python_api_encoder_encode_missing_path() -> None:
    try:
        from libsixel_wheel import sixel_encoder_encode
        from libsixel_wheel import sixel_encoder_new
        from libsixel_wheel import sixel_encoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    encoder = sixel_encoder_new()
    rejected = False
    try:
        sixel_encoder_encode(encoder, "this-file-should-not-exist.png")
    except RuntimeError:
        rejected = True
    sixel_encoder_unref(encoder)

    if not rejected:
        raise SystemExit("encoder encode accepted a missing path")

    print("encoder encode missing-path validation verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0051_python_api_encoder_encode_missing_path))
