#!/usr/bin/env python3
"""TAP test that output callback receives priv object from sixel_output_new."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'output callback receives priv object from sixel_output_new'


def test_0069_python_api_output_new_priv_passthrough() -> None:
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

    chunks = []
    priv_obj = {'name': 'priv-marker'}

    def _write(data: bytes, priv: object) -> None:
        if priv is not priv_obj:
            raise SystemExit('output callback priv was not preserved')
        chunks.append(data)

    output = sixel_output_new(_write, priv=priv_obj)
    dither = sixel_dither_get(SIXEL_BUILTIN_XTERM256)
    pixels = bytes([255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255])
    depth = sixel_helper_compute_depth(SIXEL_PIXELFORMAT_RGB888)
    status = sixel_encode(pixels, 2, 2, depth, dither, output)
    sixel_output_unref(output)

    if status != 0:
        raise SystemExit(f'sixel_encode failed with status {status}')

    if not chunks:
        raise SystemExit('output callback was not invoked')

    print('output callback priv passthrough verified')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0069_python_api_output_new_priv_passthrough,
    ))
