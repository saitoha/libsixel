#!/usr/bin/env python3
"""TAP test that callback exception state is cleared after it is raised once."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'output callback exception state clears after first raised error'


def test_0142_python_api_output_callback_first_failure_second_success() -> None:
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

    state = {'count': 0}

    def _write(_data: bytes, _priv: object) -> None:
        state['count'] += 1
        if state['count'] == 1:
            raise RuntimeError('first callback failure')

    output = sixel_output_new(_write)
    dither = sixel_dither_get(SIXEL_BUILTIN_XTERM256)
    depth = sixel_helper_compute_depth(SIXEL_PIXELFORMAT_RGB888)

    try:
        sixel_encode(b'\xff\x00\x00', 1, 1, depth, dither, output)
    except RuntimeError:
        pass
    else:
        sixel_output_unref(output)
        raise SystemExit('first encode did not raise callback RuntimeError')

    status = sixel_encode(b'\xff\x00\x00', 1, 1, depth, dither, output)
    sixel_output_unref(output)

    if status != 0:
        raise SystemExit(f'second encode failed unexpectedly with status {status}')
    if state['count'] < 2:
        raise SystemExit('callback was not invoked on second encode')

    print('output callback exception state clear-after-raise verified')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0142_python_api_output_callback_first_failure_second_success,
    ))
