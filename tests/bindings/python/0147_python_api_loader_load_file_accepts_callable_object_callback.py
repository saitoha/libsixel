#!/usr/bin/env python3
"""TAP test that loader_load_file accepts callable-object callbacks."""

from __future__ import annotations

import os

from _taptest import run_embedded_tap_test


DESCRIPTION = 'loader_load_file accepts callable-object callback'


def test_0147_python_api_loader_load_file_accepts_callable_object_callback() -> None:
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

    class _LoadCallback:
        def __init__(self) -> None:
            self.calls = 0

        def __call__(self, _frame: object, _context: object) -> int:
            self.calls += 1
            return 0

    callback = _LoadCallback()
    loader = sixel_loader_new()
    try:
        sixel_loader_load_file(loader, str(source), callback)
    finally:
        sixel_loader_unref(loader)

    if callback.calls <= 0:
        raise SystemExit('callable-object loader callback was not invoked')

    print('loader callable-object callback acceptance verified')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0147_python_api_loader_load_file_accepts_callable_object_callback,
    ))
