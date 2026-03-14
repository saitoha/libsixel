#!/usr/bin/env python3
"""TAP test that loader rejects True filename argument."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'loader rejects True filename argument'


def test_0162_python_api_loader_load_file_rejects_true_filename() -> None:
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
            sixel_loader_load_file(loader, True, lambda _f, _ctx: 0)
        except (AttributeError, TypeError, RuntimeError, ValueError):
            print('loader True filename rejection verified')
            return
        raise SystemExit('loader accepted True filename unexpectedly')
    finally:
        sixel_loader_unref(loader)


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0162_python_api_loader_load_file_rejects_true_filename,
    ))
