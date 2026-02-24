#!/usr/bin/env python3
"""TAP test for dither lifecycle APIs in libsixel.__init__.py."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'dither new/ref/unref lifecycle APIs are callable'
def test_0033_python_api_dither_lifecycle() -> None:
    try:
        from libsixel_wheel import sixel_dither_new
        from libsixel_wheel import sixel_dither_ref
        from libsixel_wheel import sixel_dither_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    dither = sixel_dither_new(16)
    sixel_dither_ref(dither)
    sixel_dither_unref(dither)
    sixel_dither_unref(dither)

    print("dither lifecycle APIs verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0033_python_api_dither_lifecycle))
