#!/usr/bin/env python3
"""TAP test that callback exceptions still surface after output_ref usage."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'output callback exception surfaces after output_ref'


def test_0146_python_api_output_callback_exception_surfaces_after_output_ref() -> None:
    try:
        from libsixel_wheel import SIXEL_BUILTIN_XTERM256
        from libsixel_wheel import SIXEL_PIXELFORMAT_RGB888
        from libsixel_wheel import sixel_dither_get
        from libsixel_wheel import sixel_encode
        from libsixel_wheel import sixel_helper_compute_depth
        from libsixel_wheel import sixel_output_new
        from libsixel_wheel import sixel_output_ref
        from libsixel_wheel import sixel_output_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    dither = sixel_dither_get(SIXEL_BUILTIN_XTERM256)
    depth = sixel_helper_compute_depth(SIXEL_PIXELFORMAT_RGB888)
    output = sixel_output_new(
        lambda _data, _priv: (_ for _ in ()).throw(RuntimeError('boom after ref'))
    )
    sixel_output_ref(output)

    try:
        try:
            sixel_encode(b'\xff\x00\x00', 1, 1, depth, dither, output)
        except RuntimeError:
            print('output_ref callback exception propagation verified')
            return

        raise SystemExit('callback exception did not surface after output_ref')
    finally:
        sixel_output_unref(output)
        sixel_output_unref(output)


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0146_python_api_output_callback_exception_surfaces_after_output_ref,
    ))
