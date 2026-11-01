#!/usr/bin/env python3
"""TAP test that loader setopt accepts bytes value for loader order."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'loader setopt accepts bytes value for loader order'
def test_0057_python_api_loader_setopt_loader_order_bytes() -> None:
    try:
        from libsixel_wheel import SIXEL_LOADER_OPTION_LOADER_ORDER
        from libsixel_wheel import sixel_loader_new
        from libsixel_wheel import sixel_loader_setopt
        from libsixel_wheel import sixel_loader_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    loader = sixel_loader_new()
    sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOADER_ORDER, b"builtin")
    sixel_loader_unref(loader)

    print("loader order bytes-input path verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0057_python_api_loader_setopt_loader_order_bytes))
