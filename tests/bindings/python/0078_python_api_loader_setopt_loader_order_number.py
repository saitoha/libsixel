#!/usr/bin/env python3
"""TAP test that loader setopt accepts numeric loader-order input."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'loader setopt accepts numeric loader-order input'


def test_0078_python_api_loader_setopt_loader_order_number() -> None:
    try:
        from libsixel_wheel import SIXEL_LOADER_OPTION_LOADER_ORDER
        from libsixel_wheel import sixel_loader_new
        from libsixel_wheel import sixel_loader_setopt
        from libsixel_wheel import sixel_loader_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    loader = sixel_loader_new()
    sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOADER_ORDER, 123)
    sixel_loader_unref(loader)

    print('loader-order numeric conversion path verified')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0078_python_api_loader_setopt_loader_order_number,
    ))
