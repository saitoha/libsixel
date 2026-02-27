#!/usr/bin/env python3
"""TAP test that loader load_file rejects bytearray filename input."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'loader load_file rejects bytearray filename input'


def test_0113_python_api_loader_load_file_rejects_bytearray_filename() -> None:
    try:
        from libsixel_wheel import sixel_loader_load_file
        from libsixel_wheel import sixel_loader_new
        from libsixel_wheel import sixel_loader_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    loader = sixel_loader_new()

    def _fn_load(_frame: object, _context: object) -> int:
        return 0

    try:
        sixel_loader_load_file(loader, bytearray(b'dummy.png'), _fn_load)
    except AttributeError:
        sixel_loader_unref(loader)
        print('loader bytearray filename rejection verified')
        return

    sixel_loader_unref(loader)
    raise SystemExit('loader accepted bytearray filename input')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0113_python_api_loader_load_file_rejects_bytearray_filename,
    ))
