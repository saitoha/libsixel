#!/usr/bin/env python3
"""TAP test for dither palette getter/setter APIs."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'dither palette getter and setter APIs are callable'
def test_0041_python_api_dither_palette() -> None:
    try:
        from libsixel_wheel import sixel_dither_get
        from libsixel_wheel import sixel_dither_get_palette
        from libsixel_wheel import sixel_dither_set_palette
        from libsixel_wheel import SIXEL_BUILTIN_XTERM16
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    dither = sixel_dither_get(SIXEL_BUILTIN_XTERM16)
    palette = sixel_dither_get_palette(dither)
    if not palette:
        raise SystemExit("palette getter returned empty data")

    # Set the same palette back to cover the setter path.
    sixel_dither_set_palette(dither, palette)

    print(f"dither palette APIs verified ({len(palette)} bytes)")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0041_python_api_dither_palette))
