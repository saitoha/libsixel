#!/usr/bin/env python3
"""TAP test that dither palette color count getter returns positive value."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'dither palette color count getter returns positive value'
def test_0058_python_api_dither_palette_color_count() -> None:
    try:
        from libsixel_wheel import SIXEL_BUILTIN_XTERM256
        from libsixel_wheel import sixel_dither_get
        from libsixel_wheel import sixel_dither_get_num_of_palette_colors
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    dither = sixel_dither_get(SIXEL_BUILTIN_XTERM256)
    count = sixel_dither_get_num_of_palette_colors(dither)
    if count <= 0:
        raise SystemExit("palette color count is not positive")

    print(f"palette color count verified ({count})")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0058_python_api_dither_palette_color_count))
