#!/usr/bin/env python3
"""TAP test that loader_load_file accepts pathlib.Path filename input."""

from __future__ import annotations

import os

from _taptest import run_embedded_tap_test


DESCRIPTION = 'loader_load_file accepts pathlib.Path filename input'


def test_0105_python_api_loader_load_file_accepts_pathlike_filename() -> None:
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
    callbacks = 0

    def _fn_load(_frame: object, _context: object) -> int:
        nonlocal callbacks
        callbacks += 1
        return 0

    loader = sixel_loader_new()
    sixel_loader_load_file(loader, source, _fn_load)
    sixel_loader_unref(loader)

    if callbacks == 0:
        raise SystemExit('loader_load_file callback was not invoked')

    print('loader_load_file path-like filename acceptance verified')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0105_python_api_loader_load_file_accepts_pathlike_filename,
    ))
