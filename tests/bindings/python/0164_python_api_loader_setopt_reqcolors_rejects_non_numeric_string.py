#!/usr/bin/env python3
"""TAP test that loader setopt rejects non-numeric reqcolors string."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'loader setopt rejects non-numeric reqcolors string'


def test_0167_python_api_loader_setopt_reqcolors_rejects_non_numeric_string() -> None:
    try:
        from libsixel_wheel import SIXEL_LOADER_OPTION_REQCOLORS
        from libsixel_wheel import sixel_loader_new
        from libsixel_wheel import sixel_loader_setopt
        from libsixel_wheel import sixel_loader_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    loader = sixel_loader_new()
    try:
        sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_REQCOLORS, "abc")
    except (TypeError, ValueError):
        sixel_loader_unref(loader)
        print("loader reqcolors non-numeric rejection verified")
        return

    sixel_loader_unref(loader)
    raise SystemExit("loader setopt accepted non-numeric reqcolors string")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0167_python_api_loader_setopt_reqcolors_rejects_non_numeric_string,
    ))
