#!/usr/bin/env python3
"""TAP test that loader_load_file accepts c_void_p callback sentinel type."""

from __future__ import annotations

from ctypes import c_void_p

from _taptest import run_embedded_tap_test


DESCRIPTION = 'loader_load_file accepts c_void_p callback sentinel type'


def test_0151_python_api_loader_load_file_accepts_null_pointer_callback() -> None:
    try:
        from libsixel_wheel import sixel_loader_load_file
        from libsixel_wheel import sixel_loader_new
        from libsixel_wheel import sixel_loader_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    loader = sixel_loader_new()
    try:
        try:
            sixel_loader_load_file(loader, b'dummy.png', c_void_p(0))
        except (RuntimeError, ValueError, TypeError):
            print('loader callback c_void_p sentinel was accepted by wrapper')
            return

        raise SystemExit('loader callback c_void_p sentinel unexpectedly succeeded')
    finally:
        sixel_loader_unref(loader)


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0151_python_api_loader_load_file_accepts_null_pointer_callback,
    ))
