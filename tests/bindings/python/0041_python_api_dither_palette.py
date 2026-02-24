#!/usr/bin/env python3
"""TAP test for dither palette getter/setter APIs."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'dither palette getter and setter APIs are callable'
def test_0041_python_api_dither_palette() -> None:
    try:
        from libsixel_wheel import sixel_dither_destroy
        from libsixel_wheel import sixel_dither_get_palette
        from libsixel_wheel import sixel_dither_new
        from libsixel_wheel import sixel_dither_set_palette
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    dither = sixel_dither_new(2)
    palette = [1, 2, 3, 4, 5, 6]
    sixel_dither_set_palette(dither, palette)

    actual_palette = sixel_dither_get_palette(dither)
    sixel_dither_destroy(dither)

    if actual_palette[:len(palette)] != palette:
        raise SystemExit('palette getter returned unexpected data')

    print(f"dither palette APIs verified ({len(actual_palette)} bytes)")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0041_python_api_dither_palette))
