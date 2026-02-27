#!/usr/bin/env python3
"""TAP test that loader setopt bgcolor rejects overflow component values."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'loader setopt bgcolor rejects overflow component values'


def test_0125_python_api_loader_setopt_bgcolor_rejects_overflow_component() -> None:
    try:
        from libsixel_wheel import SIXEL_LOADER_OPTION_BGCOLOR
        from libsixel_wheel import sixel_loader_new
        from libsixel_wheel import sixel_loader_setopt
        from libsixel_wheel import sixel_loader_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    loader = sixel_loader_new()

    try:
        sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_BGCOLOR, (0, 0, 256))
    except ValueError:
        sixel_loader_unref(loader)
        print('loader bgcolor overflow component rejection verified')
        return

    sixel_loader_unref(loader)
    raise SystemExit('loader accepted overflow bgcolor component value')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0125_python_api_loader_setopt_bgcolor_rejects_overflow_component,
    ))
