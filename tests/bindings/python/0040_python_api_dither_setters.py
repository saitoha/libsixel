#!/usr/bin/env python3
"""TAP test that dither setter APIs accept expected argument values."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'dither setter APIs accept expected argument values'
def test_0040_python_api_dither_setters() -> None:
    try:
        from libsixel_wheel import SIXEL_DIFFUSE_ATKINSON
        from libsixel_wheel import SIXEL_PIXELFORMAT_RGB888
        from libsixel_wheel import SIXEL_SCAN_SERPENTINE
        from libsixel_wheel import sixel_dither_new
        from libsixel_wheel import sixel_dither_set_body_only
        from libsixel_wheel import sixel_dither_set_complexion_score
        from libsixel_wheel import sixel_dither_set_diffusion_scan
        from libsixel_wheel import sixel_dither_set_diffusion_type
        from libsixel_wheel import sixel_dither_set_optimize_palette
        from libsixel_wheel import sixel_dither_set_pixelformat
        from libsixel_wheel import sixel_dither_set_transparent
        from libsixel_wheel import sixel_dither_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    dither = sixel_dither_new(16)
    sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_ATKINSON)
    sixel_dither_set_diffusion_scan(dither, SIXEL_SCAN_SERPENTINE)
    sixel_dither_set_complexion_score(dither, 1)
    sixel_dither_set_body_only(dither, 0)
    sixel_dither_set_optimize_palette(dither, 1)
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGB888)
    sixel_dither_set_transparent(dither, 0)
    sixel_dither_unref(dither)

    print("dither setter APIs verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0040_python_api_dither_setters))
