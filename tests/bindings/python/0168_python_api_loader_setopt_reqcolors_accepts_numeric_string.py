#!/usr/bin/env python3
"""TAP test that loader setopt accepts numeric-string reqcolors input."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'loader setopt accepts numeric-string reqcolors input'


def test_0168_python_api_loader_setopt_reqcolors_accepts_numeric_string() -> None:
    try:
        from libsixel_wheel import SIXEL_LOADER_OPTION_REQCOLORS
        from libsixel_wheel import sixel_loader_new
        from libsixel_wheel import sixel_loader_setopt
        from libsixel_wheel import sixel_loader_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    loader = sixel_loader_new()
    sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_REQCOLORS, "32")
    sixel_loader_unref(loader)

    print("loader reqcolors numeric-string acceptance verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0168_python_api_loader_setopt_reqcolors_accepts_numeric_string,
    ))
