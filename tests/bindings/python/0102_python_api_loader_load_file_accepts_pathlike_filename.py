#!/usr/bin/env python3
"""TAP test for path-like filename rejection in loader_load_file."""

from __future__ import annotations

import os

from _taptest import run_embedded_tap_test


DESCRIPTION = 'loader_load_file rejects pathlib.Path filename input'


def test_0105_python_api_loader_load_file_rejects_pathlike_filename() -> None:
    import pathlib

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
        sixel_loader_load_file(loader, source, lambda _frame, _context: 0)
    except AttributeError:
        sixel_loader_unref(loader)
        print('loader_load_file path-like filename rejection verified')
        return

    sixel_loader_unref(loader)
    raise AssertionError('loader_load_file accepted pathlib.Path filename input')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0105_python_api_loader_load_file_rejects_pathlike_filename,
    ))
