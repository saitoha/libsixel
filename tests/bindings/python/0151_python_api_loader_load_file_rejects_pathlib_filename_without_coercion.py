#!/usr/bin/env python3
"""TAP test that loader_load_file rejects pathlib.Path without str coercion."""

from __future__ import annotations

import os
import pathlib

from _taptest import run_embedded_tap_test


DESCRIPTION = 'loader_load_file rejects pathlib.Path without explicit str coercion'


def test_0154_python_api_loader_load_file_rejects_pathlib_filename_without_coercion() -> None:
    try:
        from libsixel_wheel import sixel_loader_load_file
        from libsixel_wheel import sixel_loader_new
        from libsixel_wheel import sixel_loader_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    source = pathlib.Path(
        os.path.expandvars('${TOP_SRCDIR}/tests/data/inputs/snake_64.png')
    )

    loader = sixel_loader_new()
    try:
        try:
            sixel_loader_load_file(loader, source, lambda _frame, _ctx: 0)
        except (AttributeError, TypeError, RuntimeError, ValueError):
            print('loader pathlib.Path rejection without explicit str coercion verified')
            return

        raise SystemExit('loader accepted pathlib.Path without explicit str coercion')
    finally:
        sixel_loader_unref(loader)


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0154_python_api_loader_load_file_rejects_pathlib_filename_without_coercion,
    ))
