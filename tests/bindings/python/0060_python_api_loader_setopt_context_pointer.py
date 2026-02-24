#!/usr/bin/env python3
"""TAP test for c_void_p context path in sixel_loader_setopt()."""

from __future__ import annotations

import ctypes

from _taptest import run_embedded_tap_test


DESCRIPTION = 'loader setopt accepts c_void_p context values'
def test_0060_python_api_loader_setopt_context_pointer() -> None:
    try:
        from libsixel_wheel import SIXEL_LOADER_OPTION_CONTEXT
        from libsixel_wheel import sixel_loader_new
        from libsixel_wheel import sixel_loader_setopt
        from libsixel_wheel import sixel_loader_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    loader = sixel_loader_new()
    sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_CONTEXT, ctypes.c_void_p(0))
    sixel_loader_unref(loader)

    print("loader context c_void_p path verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0060_python_api_loader_setopt_context_pointer))
