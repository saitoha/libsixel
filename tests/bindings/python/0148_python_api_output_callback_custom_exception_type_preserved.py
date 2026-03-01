#!/usr/bin/env python3
"""TAP test that callback preserves custom exception type on re-raise."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'output callback preserves custom exception type on re-raise'


class _CallbackBoom(RuntimeError):
    """Custom callback exception used to verify type preservation."""


def test_0148_python_api_output_callback_custom_exception_type_preserved() -> None:
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

    output = sixel_output_new(
        lambda _data, _priv: (_ for _ in ()).throw(_CallbackBoom('typed boom'))
    )
    dither = sixel_dither_get(SIXEL_BUILTIN_XTERM256)
    depth = sixel_helper_compute_depth(SIXEL_PIXELFORMAT_RGB888)

    try:
        try:
            sixel_encode(b'\xff\x00\x00', 1, 1, depth, dither, output)
        except _CallbackBoom as exc:
            if str(exc) != 'typed boom':
                raise SystemExit('callback exception message changed during re-raise')
            print('custom callback exception type and message were preserved')
            return

        raise SystemExit('custom callback exception type was not preserved')
    finally:
        sixel_output_unref(output)


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0148_python_api_output_callback_custom_exception_type_preserved,
    ))
