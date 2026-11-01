#!/usr/bin/env python3
"""TAP test that dither get returns usable built-in dither context."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'dither get returns usable built-in dither context'
def test_0034_python_api_dither_builtin_get() -> None:
    try:
        from libsixel_wheel import SIXEL_BUILTIN_XTERM256
        from libsixel_wheel import sixel_dither_get
        from libsixel_wheel import sixel_dither_get_num_of_palette_colors
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    dither = sixel_dither_get(SIXEL_BUILTIN_XTERM256)
    if not dither:
        raise SystemExit("sixel_dither_get returned null")

    palette_colors = sixel_dither_get_num_of_palette_colors(dither)
    if palette_colors <= 0:
        raise SystemExit("built-in dither has no palette colors")

    print(f"built-in dither verified ({palette_colors} colors)")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0034_python_api_dither_builtin_get))
