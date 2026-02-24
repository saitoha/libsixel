#!/usr/bin/env python3
"""TAP test for callback validation in sixel_loader_load_file."""

from __future__ import annotations

import os

from _taptest import run_embedded_tap_test


DESCRIPTION = 'loader load_file rejects missing callback argument'
def test_0050_python_api_loader_load_file_requires_callback() -> None:
    import pathlib

    try:
        from libsixel_wheel import sixel_loader_load_file
        from libsixel_wheel import sixel_loader_new
        from libsixel_wheel import sixel_loader_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    source = pathlib.Path(os.path.expandvars("${TOP_SRCDIR}/tests/data/inputs/snake_64.png"))
    loader = sixel_loader_new()

    rejected = False
    try:
        sixel_loader_load_file(loader, str(source), None)
    except ValueError:
        rejected = True
    sixel_loader_unref(loader)

    if not rejected:
        raise SystemExit("loader load_file accepted a missing callback")

    print("loader load_file callback validation verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0050_python_api_loader_load_file_requires_callback))
