#!/usr/bin/env python3
"""TAP test that dither initialize accepts RGB buffer and updates state."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'dither initialize accepts RGB buffer and updates state'
def test_0039_python_api_dither_initialize() -> None:
    try:
        from libsixel_wheel import SIXEL_PIXELFORMAT_RGB888
        from libsixel_wheel import sixel_dither_get_num_of_histogram_colors
        from libsixel_wheel import sixel_dither_initialize
        from libsixel_wheel import sixel_dither_new
        from libsixel_wheel import sixel_dither_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    pixels = bytes([
        255, 0, 0,
        0, 255, 0,
        0, 0, 255,
        255, 255, 255,
    ])

    dither = sixel_dither_new(16)
    sixel_dither_initialize(dither, pixels, 2, 2, SIXEL_PIXELFORMAT_RGB888)
    histogram = sixel_dither_get_num_of_histogram_colors(dither)
    sixel_dither_unref(dither)

    if histogram <= 0:
        raise SystemExit("dither histogram did not initialize")

    print(f"dither initialize verified ({histogram} histogram colors)")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0039_python_api_dither_initialize))
