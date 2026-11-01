#!/usr/bin/env python3
"""TAP test that output callback receives None when priv is omitted."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'output new default priv is None in callback'


def test_0150_python_api_output_new_default_priv_is_none() -> None:
    try:
        from libsixel_wheel import SIXEL_BUILTIN_XTERM256
        from libsixel_wheel import SIXEL_PIXELFORMAT_RGB888
        from libsixel_wheel import sixel_dither_get
        from libsixel_wheel import sixel_encode
        from libsixel_wheel import sixel_helper_compute_depth
        from libsixel_wheel import sixel_output_new
        from libsixel_wheel import sixel_output_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    saw_none = False

    def _write(_data: bytes, priv: object) -> None:
        nonlocal saw_none
        saw_none = saw_none or priv is None

    output = sixel_output_new(_write)
    dither = sixel_dither_get(SIXEL_BUILTIN_XTERM256)
    depth = sixel_helper_compute_depth(SIXEL_PIXELFORMAT_RGB888)
    try:
        status = sixel_encode(b'\xff\x00\x00', 1, 1, depth, dither, output)
        if status != 0:
            raise SystemExit(f'sixel_encode failed unexpectedly: {status}')
        if not saw_none:
            raise SystemExit('callback did not receive None priv')
        print('output callback default priv(None) verified')
    finally:
        sixel_output_unref(output)


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0150_python_api_output_new_default_priv_is_none,
    ))
