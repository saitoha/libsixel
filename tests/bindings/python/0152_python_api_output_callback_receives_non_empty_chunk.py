#!/usr/bin/env python3
"""TAP test that output callback receives non-empty bytes payload."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'output callback receives non-empty bytes payload'


def test_0152_python_api_output_callback_receives_non_empty_chunk() -> None:
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

    saw_bytes = False
    saw_non_empty = False

    def _write(data: bytes, _priv: object) -> None:
        nonlocal saw_bytes
        nonlocal saw_non_empty
        saw_bytes = saw_bytes or isinstance(data, bytes)
        saw_non_empty = saw_non_empty or len(data) > 0

    output = sixel_output_new(_write)
    dither = sixel_dither_get(SIXEL_BUILTIN_XTERM256)
    depth = sixel_helper_compute_depth(SIXEL_PIXELFORMAT_RGB888)
    try:
        status = sixel_encode(b'\xff\x00\x00', 1, 1, depth, dither, output)
        if status != 0:
            raise SystemExit(f'sixel_encode failed unexpectedly: {status}')
        if not saw_bytes:
            raise SystemExit('callback payload was not bytes')
        if not saw_non_empty:
            raise SystemExit('callback payload was unexpectedly empty')
        print('output callback payload type/size verified')
    finally:
        sixel_output_unref(output)


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0152_python_api_output_callback_receives_non_empty_chunk,
    ))
