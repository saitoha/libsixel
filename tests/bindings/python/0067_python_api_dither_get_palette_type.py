#!/usr/bin/env python3
"""TAP test for dither get_palette return type."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'dither get_palette returns list of integers'
def test_0067_python_api_dither_get_palette_type() -> None:
    try:
        from libsixel_wheel import sixel_dither_destroy
        from libsixel_wheel import sixel_dither_get_palette
        from libsixel_wheel import sixel_dither_new
        from libsixel_wheel import sixel_dither_set_palette
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    dither = sixel_dither_new(2)
    sixel_dither_set_palette(dither, [1, 2, 3, 4, 5, 6])
    palette = sixel_dither_get_palette(dither)
    sixel_dither_destroy(dither)

    if not isinstance(palette, list):
        raise SystemExit('get_palette did not return list')
    if len(palette) < 6:
        raise SystemExit('get_palette returned too few entries')
    if not all(isinstance(component, int) for component in palette[:6]):
        raise SystemExit('get_palette returned non-integer component')

    print('dither get_palette return type verified')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(DESCRIPTION,
                                           test_0067_python_api_dither_get_palette_type))
