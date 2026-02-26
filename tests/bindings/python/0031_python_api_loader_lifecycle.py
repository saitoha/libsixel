#!/usr/bin/env python3
"""TAP test that loader new/ref/unref lifecycle APIs are callable."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'loader new/ref/unref lifecycle APIs are callable'
def test_0031_python_api_loader_lifecycle() -> None:
    try:
        from libsixel_wheel import sixel_loader_new
        from libsixel_wheel import sixel_loader_ref
        from libsixel_wheel import sixel_loader_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    loader = sixel_loader_new()
    sixel_loader_ref(loader)
    sixel_loader_unref(loader)
    sixel_loader_unref(loader)

    print("loader lifecycle APIs verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0031_python_api_loader_lifecycle))
