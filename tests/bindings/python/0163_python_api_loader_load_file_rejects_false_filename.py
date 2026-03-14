#!/usr/bin/env python3
"""TAP test that loader rejects False filename argument."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'loader rejects False filename argument'


def test_0163_python_api_loader_load_file_rejects_false_filename() -> None:
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
            sixel_loader_load_file(loader, False, lambda _f, _ctx: 0)
        except (AttributeError, TypeError, RuntimeError, ValueError):
            print('loader False filename rejection verified')
            return
        raise SystemExit('loader accepted False filename unexpectedly')
    finally:
        sixel_loader_unref(loader)


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0163_python_api_loader_load_file_rejects_false_filename,
    ))
