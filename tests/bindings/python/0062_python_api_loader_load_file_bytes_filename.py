#!/usr/bin/env python3
"""TAP test that loader load_file accepts bytes filename input."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'loader load_file accepts bytes filename input'
def test_0062_python_api_loader_load_file_bytes_filename() -> None:
    try:
        from libsixel_wheel import sixel_loader_load_file
        from libsixel_wheel import sixel_loader_new
        from libsixel_wheel import sixel_loader_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    loader = sixel_loader_new()
    raised_runtime_error = False
    try:
        sixel_loader_load_file(loader, b'/definitely/missing/image.png',
                               lambda _frame, _context: 0)
    except RuntimeError:
        raised_runtime_error = True
    sixel_loader_unref(loader)

    if not raised_runtime_error:
        raise SystemExit('loader_load_file did not process bytes filename path')

    print('loader load_file bytes filename path verified')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(DESCRIPTION,
                                           test_0062_python_api_loader_load_file_bytes_filename))
