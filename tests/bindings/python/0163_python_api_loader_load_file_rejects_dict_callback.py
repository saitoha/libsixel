#!/usr/bin/env python3
"""TAP test that loader rejects dict callback argument."""

from __future__ import annotations

import os

from _taptest import run_embedded_tap_test


DESCRIPTION = 'loader rejects dict callback argument'


def test_0166_python_api_loader_load_file_rejects_dict_callback() -> None:
    try:
        from libsixel_wheel import sixel_loader_load_file
        from libsixel_wheel import sixel_loader_new
        from libsixel_wheel import sixel_loader_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    source = os.path.join(os.environ.get('TOP_SRCDIR', os.getcwd()), 'tests', 'data', 'inputs', 'snake_64.png')
    loader = sixel_loader_new()
    try:
        try:
            result = sixel_loader_load_file(loader, source, {'k': 'v'})
        except (AttributeError, TypeError, RuntimeError, ValueError):
            print('loader dict callback rejection verified')
            return
        if result is None or result != 0:
            print('loader dict callback non-callable path observed')
            return
        raise SystemExit('loader accepted dict callback unexpectedly')
    finally:
        sixel_loader_unref(loader)


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0166_python_api_loader_load_file_rejects_dict_callback,
    ))
