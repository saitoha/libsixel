#!/usr/bin/env python3
"""TAP test that output callback fails when write_proc is non-callable."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'output callback rejects non-callable write_proc'


def test_0137_python_api_output_new_rejects_non_callable_write_proc() -> None:
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

    output = sixel_output_new(123)
    dither = sixel_dither_get(SIXEL_BUILTIN_XTERM256)
    depth = sixel_helper_compute_depth(SIXEL_PIXELFORMAT_RGB888)
    try:
        sixel_encode(b'\xff\x00\x00', 1, 1, depth, dither, output)
    except (RuntimeError, TypeError):
        sixel_output_unref(output)
        print('output non-callable write_proc rejection verified')
        return

    sixel_output_unref(output)
    raise SystemExit('output accepted non-callable write_proc during callback')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0137_python_api_output_new_rejects_non_callable_write_proc,
    ))
