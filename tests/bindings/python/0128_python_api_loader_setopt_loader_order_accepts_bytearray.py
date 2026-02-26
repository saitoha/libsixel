#!/usr/bin/env python3
"""TAP test for loader-order bytearray conversion in loader setopt."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'loader setopt accepts bytearray loader-order input via string conversion'


def test_0128_python_api_loader_setopt_loader_order_accepts_bytearray() -> None:
    try:
        from libsixel_wheel import SIXEL_LOADER_OPTION_LOADER_ORDER
        from libsixel_wheel import sixel_loader_new
        from libsixel_wheel import sixel_loader_setopt
        from libsixel_wheel import sixel_loader_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    loader = sixel_loader_new()
    sixel_loader_setopt(
        loader,
        SIXEL_LOADER_OPTION_LOADER_ORDER,
        bytearray(b'stb,png'),
    )
    sixel_loader_unref(loader)

    print('loader-order bytearray conversion path verified')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0128_python_api_loader_setopt_loader_order_accepts_bytearray,
    ))
