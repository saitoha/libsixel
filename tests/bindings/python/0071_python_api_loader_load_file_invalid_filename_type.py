#!/usr/bin/env python3
"""TAP test for invalid filename type handling in loader_load_file()."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'loader_load_file rejects non-string filename objects'


def test_0075_python_api_loader_load_file_invalid_filename_type() -> None:
    try:
        from libsixel_wheel import sixel_loader_load_file
        from libsixel_wheel import sixel_loader_new
        from libsixel_wheel import sixel_loader_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    loader = sixel_loader_new()

    try:
        sixel_loader_load_file(loader, 123, lambda _frame, _context: 0)
    except (TypeError, AttributeError):
        sixel_loader_unref(loader)
        print('loader non-string filename rejection verified')
        return

    sixel_loader_unref(loader)
    raise AssertionError('loader accepted non-string filename')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0075_python_api_loader_load_file_invalid_filename_type,
    ))
