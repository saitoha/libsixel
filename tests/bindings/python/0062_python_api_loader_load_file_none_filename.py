#!/usr/bin/env python3
"""TAP test that loader load_file rejects None filename without crashing."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'loader load_file rejects None filename without crashing'
def test_0065_python_api_loader_load_file_none_filename() -> None:
    try:
        from libsixel_wheel import sixel_loader_load_file
        from libsixel_wheel import sixel_loader_new
        from libsixel_wheel import sixel_loader_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    loader = sixel_loader_new()

    try:
        sixel_loader_load_file(loader, None, lambda _frame, _context: 0)
    except TypeError:
        sixel_loader_unref(loader)
        print('loader load_file None-filename rejection verified')
        return

    sixel_loader_unref(loader)
    raise SystemExit('loader load_file accepted None filename')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(DESCRIPTION,
                                           test_0065_python_api_loader_load_file_none_filename))
