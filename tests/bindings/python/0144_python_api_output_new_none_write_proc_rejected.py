#!/usr/bin/env python3
"""TAP test that output callback configured with None is rejected on encode."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'output callback configured with None is rejected on encode'


def test_0144_python_api_output_new_none_write_proc_rejected() -> None:
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

    output = sixel_output_new(None)
    dither = sixel_dither_get(SIXEL_BUILTIN_XTERM256)
    depth = sixel_helper_compute_depth(SIXEL_PIXELFORMAT_RGB888)
    try:
        sixel_encode(b'\xff\x00\x00', 1, 1, depth, dither, output)
    except TypeError:
        sixel_output_unref(output)
        print('output None callback rejection verified')
        return

    sixel_output_unref(output)
    raise SystemExit('output callback None was accepted during encode')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0144_python_api_output_new_none_write_proc_rejected,
    ))
