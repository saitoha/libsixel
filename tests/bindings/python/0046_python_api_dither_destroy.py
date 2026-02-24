#!/usr/bin/env python3
"""TAP test for sixel_dither_destroy()."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'dither destroy API is callable'
def test_0046_python_api_dither_destroy() -> None:
    try:
        from libsixel_wheel import sixel_dither_destroy
        from libsixel_wheel import sixel_dither_new
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    dither = sixel_dither_new(16)
    sixel_dither_destroy(dither)

    print("dither destroy verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0046_python_api_dither_destroy))
