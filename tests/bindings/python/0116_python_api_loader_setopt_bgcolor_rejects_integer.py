#!/usr/bin/env python3
"""TAP test that loader setopt rejects integer bgcolor input."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'loader setopt rejects integer bgcolor input'


def test_0116_python_api_loader_setopt_bgcolor_rejects_integer() -> None:
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
        sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_BGCOLOR, 1)
    except TypeError:
        sixel_loader_unref(loader)
        print('loader integer bgcolor rejection verified')
        return

    sixel_loader_unref(loader)
    raise SystemExit('loader accepted integer bgcolor input')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0116_python_api_loader_setopt_bgcolor_rejects_integer,
    ))
