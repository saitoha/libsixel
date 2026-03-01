#!/usr/bin/env python3
"""TAP test that callback failures remain isolated to each output handle."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'output callback failure is isolated per output handle'


def test_0145_python_api_output_callback_failure_isolated_per_output() -> None:
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

    dither = sixel_dither_get(SIXEL_BUILTIN_XTERM256)
    depth = sixel_helper_compute_depth(SIXEL_PIXELFORMAT_RGB888)

    failing = sixel_output_new(
        lambda _data, _priv: (_ for _ in ()).throw(RuntimeError('first boom'))
    )
    succeeding_calls = 0

    def _ok_write(_data: bytes, _priv: object) -> None:
        nonlocal succeeding_calls
        succeeding_calls += 1

    succeeding = sixel_output_new(_ok_write)

    try:
        try:
            sixel_encode(b'\xff\x00\x00', 1, 1, depth, dither, failing)
        except RuntimeError:
            pass
        else:
            raise SystemExit('failing output did not surface callback exception')

        status = sixel_encode(b'\x00\xff\x00', 1, 1, depth, dither, succeeding)
        if status != 0:
            raise SystemExit(f'second output encode failed unexpectedly: {status}')

        if succeeding_calls <= 0:
            raise SystemExit('second output callback was not invoked')

        print('output callback exception state isolation verified')
    finally:
        sixel_output_unref(succeeding)
        sixel_output_unref(failing)


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0145_python_api_output_callback_failure_isolated_per_output,
    ))
