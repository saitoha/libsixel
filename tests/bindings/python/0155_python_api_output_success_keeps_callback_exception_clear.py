#!/usr/bin/env python3
"""TAP test that successful output callback leaves exception state clear."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'successful output callback keeps callback exception state clear'


def test_0155_python_api_output_success_keeps_callback_exception_clear() -> None:
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

    output = sixel_output_new(lambda _data, _priv: None)
    dither = sixel_dither_get(SIXEL_BUILTIN_XTERM256)
    depth = sixel_helper_compute_depth(SIXEL_PIXELFORMAT_RGB888)

    try:
        status = sixel_encode(b'\xff\x00\x00', 1, 1, depth, dither, output)
        if status != 0:
            raise SystemExit(f'sixel_encode failed unexpectedly: {status}')

        if getattr(output, '__callback_exception', 'missing') is not None:
            raise SystemExit('callback exception state was populated on successful callback')

        print('successful callback kept callback exception state clear')
    finally:
        sixel_output_unref(output)


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0155_python_api_output_success_keeps_callback_exception_clear,
    ))
