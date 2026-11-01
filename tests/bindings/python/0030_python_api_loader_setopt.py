#!/usr/bin/env python3
"""TAP test that loader setopt call paths accept integer and string payloads."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'loader setopt call paths accept integer and string payloads'
def test_0032_python_api_loader_setopt() -> None:
    try:
        from libsixel_wheel import SIXEL_LOADER_OPTION_LOADER_ORDER
        from libsixel_wheel import SIXEL_LOADER_OPTION_REQCOLORS
        from libsixel_wheel import sixel_loader_new
        from libsixel_wheel import sixel_loader_setopt
        from libsixel_wheel import sixel_loader_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    loader = sixel_loader_new()
    sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_REQCOLORS, 32)
    sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOADER_ORDER, "builtin")
    sixel_loader_unref(loader)

    print("loader setopt verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0032_python_api_loader_setopt))
