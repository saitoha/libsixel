#!/usr/bin/env python3
"""TAP test that ValueError from output callback surfaces to Python caller."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'output callback ValueError surfaces to Python caller'


def test_0141_python_api_output_callback_valueerror_surfaces() -> None:
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

    output = sixel_output_new(lambda _data, _priv: (_ for _ in ()).throw(ValueError('callback value error')))
    dither = sixel_dither_get(SIXEL_BUILTIN_XTERM256)
    depth = sixel_helper_compute_depth(SIXEL_PIXELFORMAT_RGB888)
    try:
        sixel_encode(b'\xff\x00\x00', 1, 1, depth, dither, output)
    except ValueError:
        sixel_output_unref(output)
        print('output callback ValueError propagation verified')
        return

    sixel_output_unref(output)
    raise SystemExit('output callback ValueError did not surface')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0141_python_api_output_callback_valueerror_surfaces,
    ))
