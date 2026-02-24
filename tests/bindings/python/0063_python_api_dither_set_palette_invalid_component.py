#!/usr/bin/env python3
"""TAP test for dither palette component validation."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'dither set_palette rejects out-of-range components'
def test_0063_python_api_dither_set_palette_invalid_component() -> None:
    try:
        from libsixel_wheel import sixel_dither_destroy
        from libsixel_wheel import sixel_dither_new
        from libsixel_wheel import sixel_dither_set_palette
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    dither = sixel_dither_new(2)
    rejected = False
    try:
        sixel_dither_set_palette(dither, [0, 0, 0, 256, 0, 0])
    except ValueError:
        rejected = True
    sixel_dither_destroy(dither)

    if not rejected:
        raise SystemExit('set_palette accepted out-of-range component')

    print('dither set_palette invalid-component path verified')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(DESCRIPTION,
                                           test_0063_python_api_dither_set_palette_invalid_component))
