#!/usr/bin/env python3
"""TAP test for sixel_encode() in libsixel.__init__.py."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'sixel_encode writes sixel bytes through output context'
def test_0036_python_api_sixel_encode_function() -> None:
    try:
        from libsixel_wheel import SIXEL_BUILTIN_XTERM256
        from libsixel_wheel import SIXEL_PIXELFORMAT_RGB888
        from libsixel_wheel import SIXEL_SUCCEEDED
        from libsixel_wheel import sixel_dither_get
        from libsixel_wheel import sixel_encode
        from libsixel_wheel import sixel_helper_compute_depth
        from libsixel_wheel import sixel_output_new
        from libsixel_wheel import sixel_output_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    chunks = []

    def _write(data: bytes, _priv: object) -> None:
        chunks.append(data)

    output = sixel_output_new(_write)
    dither = sixel_dither_get(SIXEL_BUILTIN_XTERM256)

    pixels = bytes([
        255, 0, 0,
        0, 255, 0,
        0, 0, 255,
        255, 255, 255,
    ])
    depth = sixel_helper_compute_depth(SIXEL_PIXELFORMAT_RGB888)
    status = sixel_encode(pixels, 2, 2, depth, dither, output)
    sixel_output_unref(output)

    if not SIXEL_SUCCEEDED(status):
        raise SystemExit(f"sixel_encode failed with status {status}")

    payload = b"".join(chunks)
    if not payload.startswith(b"\x1bP"):
        raise SystemExit("encoded output missing sixel introducer")

    print("sixel_encode verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0036_python_api_sixel_encode_function))
