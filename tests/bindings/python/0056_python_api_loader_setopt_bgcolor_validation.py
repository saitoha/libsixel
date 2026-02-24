#!/usr/bin/env python3
"""TAP test for bgcolor validation in sixel_loader_setopt()."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'loader setopt rejects invalid bgcolor tuple length'
def test_0056_python_api_loader_setopt_bgcolor_validation() -> None:
    try:
        from libsixel_wheel import SIXEL_LOADER_OPTION_BGCOLOR
        from libsixel_wheel import sixel_loader_new
        from libsixel_wheel import sixel_loader_setopt
        from libsixel_wheel import sixel_loader_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    loader = sixel_loader_new()
    rejected = False
    try:
        sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_BGCOLOR, (1, 2))
    except ValueError:
        rejected = True
    sixel_loader_unref(loader)

    if not rejected:
        raise SystemExit("loader setopt accepted invalid bgcolor length")

    print("loader bgcolor validation path verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0056_python_api_loader_setopt_bgcolor_validation))
