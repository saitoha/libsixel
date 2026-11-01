#!/usr/bin/env python3
"""TAP test that output.new accepts callable object as write callback."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'output new accepts callable object write callback'


def test_0143_python_api_output_new_accepts_callable_object() -> None:
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

    class Writer:
        def __init__(self) -> None:
            self.calls = 0

        def __call__(self, _data: bytes, _priv: object) -> None:
            self.calls += 1

    writer = Writer()
    output = sixel_output_new(writer)
    dither = sixel_dither_get(SIXEL_BUILTIN_XTERM256)
    depth = sixel_helper_compute_depth(SIXEL_PIXELFORMAT_RGB888)
    status = sixel_encode(b'\xff\x00\x00', 1, 1, depth, dither, output)
    sixel_output_unref(output)

    if status != 0:
        raise SystemExit(f'sixel_encode failed with status {status}')
    if writer.calls == 0:
        raise SystemExit('callable object callback was not invoked')

    print('output callable object callback acceptance verified')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0143_python_api_output_new_accepts_callable_object,
    ))
