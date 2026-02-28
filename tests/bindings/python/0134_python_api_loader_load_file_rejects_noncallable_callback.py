#!/usr/bin/env python3
"""TAP test that loader load_file rejects non-callable callback object."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'loader load_file rejects non-callable callback object'


def test_0134_python_api_loader_load_file_rejects_noncallable_callback() -> None:
    try:
        from libsixel_wheel import sixel_loader_load_file
        from libsixel_wheel import sixel_loader_new
        from libsixel_wheel import sixel_loader_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    loader = sixel_loader_new()
    try:
        sixel_loader_load_file(loader, b'dummy.png', object())
    except (RuntimeError, ValueError, TypeError):
        sixel_loader_unref(loader)
        print('loader non-callable callback rejection verified')
        return

    sixel_loader_unref(loader)
    raise SystemExit('loader accepted non-callable callback object')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0134_python_api_loader_load_file_rejects_noncallable_callback,
    ))
