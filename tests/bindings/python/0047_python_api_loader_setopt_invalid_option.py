#!/usr/bin/env python3
"""TAP test that loader setopt rejects unknown option values."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'loader setopt rejects unknown option values'
def test_0049_python_api_loader_setopt_invalid_option() -> None:
    try:
        from libsixel_wheel import sixel_loader_new
        from libsixel_wheel import sixel_loader_setopt
        from libsixel_wheel import sixel_loader_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    loader = sixel_loader_new()
    rejected = False
    try:
        sixel_loader_setopt(loader, 9999, 1)
    except ValueError:
        rejected = True
    sixel_loader_unref(loader)

    if not rejected:
        raise SystemExit("loader setopt accepted an unknown option")

    print("loader setopt unknown-option path verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0049_python_api_loader_setopt_invalid_option))
