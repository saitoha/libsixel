#!/usr/bin/env python3
"""TAP test that loader missing filename with valid callback surfaces runtime error."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'loader missing filename with valid callback surfaces runtime error'


def test_0156_python_api_loader_load_file_missing_filename_with_valid_callback_surfaces_runtime_error() -> None:
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
            sixel_loader_load_file(loader, b'__missing__0156__.png', lambda _f, _ctx: 0)
        except RuntimeError:
            print('loader missing filename runtime error path verified')
            return
        raise SystemExit('loader missing filename unexpectedly succeeded')
    finally:
        sixel_loader_unref(loader)


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0156_python_api_loader_load_file_missing_filename_with_valid_callback_surfaces_runtime_error,
    ))
