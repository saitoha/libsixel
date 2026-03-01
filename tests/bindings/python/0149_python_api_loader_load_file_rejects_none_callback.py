#!/usr/bin/env python3
"""TAP test that loader_load_file rejects None callback early."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'loader_load_file rejects None callback'


def test_0149_python_api_loader_load_file_rejects_none_callback() -> None:
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
            sixel_loader_load_file(loader, b'dummy.png', None)
        except ValueError:
            print('loader None callback rejection verified')
            return

        raise SystemExit('loader accepted None callback unexpectedly')
    finally:
        sixel_loader_unref(loader)


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0149_python_api_loader_load_file_rejects_none_callback,
    ))
