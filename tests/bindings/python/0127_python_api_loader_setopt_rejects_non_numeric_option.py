#!/usr/bin/env python3
"""TAP test that loader setopt rejects non-numeric option identifier."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'loader setopt rejects non-numeric option identifier'


def test_0127_python_api_loader_setopt_rejects_non_numeric_option() -> None:
    try:
        from libsixel_wheel import sixel_loader_new
        from libsixel_wheel import sixel_loader_setopt
        from libsixel_wheel import sixel_loader_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    loader = sixel_loader_new()

    try:
        sixel_loader_setopt(loader, 'loader-order', 'stb,png')
    except ValueError:
        sixel_loader_unref(loader)
        print('loader non-numeric option rejection verified')
        return

    sixel_loader_unref(loader)
    raise SystemExit('loader accepted non-numeric option identifier')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0127_python_api_loader_setopt_rejects_non_numeric_option,
    ))
