#!/usr/bin/env python3
"""TAP test that loader load_file invokes callback for source image."""

from __future__ import annotations

import os

from _taptest import run_embedded_tap_test


DESCRIPTION = 'loader load_file invokes callback for source image'
def test_0042_python_api_loader_load_file() -> None:
    import pathlib

    try:
        from libsixel_wheel import sixel_loader_load_file
        from libsixel_wheel import sixel_loader_new
        from libsixel_wheel import sixel_loader_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    source = pathlib.Path(os.path.expandvars("${TOP_SRCDIR}/tests/data/inputs/snake_64.png"))
    callback_count = 0

    def _load(_frame_ptr: object, _context_ptr: object) -> int:
        nonlocal callback_count
        callback_count += 1
        return 0

    loader = sixel_loader_new()
    sixel_loader_load_file(loader, str(source), _load)
    sixel_loader_unref(loader)

    if callback_count <= 0:
        raise SystemExit("loader callback was not invoked")

    print(f"loader callback verified ({callback_count} frame callbacks)")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0042_python_api_loader_load_file))
