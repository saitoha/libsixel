#!/usr/bin/env python3
"""TAP test that loader setopt accepts bgcolor and pointer-style options."""

from __future__ import annotations

import ctypes

from _taptest import run_embedded_tap_test


DESCRIPTION = 'loader setopt accepts bgcolor and pointer-style options'
def test_0047_python_api_loader_setopt_variants() -> None:
    try:
        from libsixel_wheel import SIXEL_LOADER_OPTION_BGCOLOR
        from libsixel_wheel import SIXEL_LOADER_OPTION_CANCEL_FLAG
        from libsixel_wheel import SIXEL_LOADER_OPTION_CONTEXT
        from libsixel_wheel import sixel_loader_new
        from libsixel_wheel import sixel_loader_setopt
        from libsixel_wheel import sixel_loader_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    cancel_flag = ctypes.c_int(0)

    loader = sixel_loader_new()
    sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_BGCOLOR, (1, 2, 3))
    sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_CANCEL_FLAG, ctypes.byref(cancel_flag))
    sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_CONTEXT, 0)
    sixel_loader_unref(loader)

    print("loader setopt variant paths verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0047_python_api_loader_setopt_variants))
