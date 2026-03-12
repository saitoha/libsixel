#!/usr/bin/env python3
"""TAP test that output callback priv is preserved across repeated encode."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'output callback priv persists across repeated encode'


def test_0153_python_api_output_callback_priv_persists_across_repeated_encode() -> None:
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

    priv = object()
    calls = 0
    saw_same_priv = True

    def _write(_data: bytes, callback_priv: object) -> None:
        nonlocal calls
        nonlocal saw_same_priv
        calls += 1
        saw_same_priv = saw_same_priv and callback_priv is priv

    output = sixel_output_new(_write, priv)
    dither = sixel_dither_get(SIXEL_BUILTIN_XTERM256)
    depth = sixel_helper_compute_depth(SIXEL_PIXELFORMAT_RGB888)
    try:
        first = sixel_encode(b'\xff\x00\x00', 1, 1, depth, dither, output)
        second = sixel_encode(b'\xff\x00\x00', 1, 1, depth, dither, output)
        if first != 0:
            raise SystemExit(f'first sixel_encode failed unexpectedly: {first}')
        if second != 0:
            raise SystemExit(f'second sixel_encode failed unexpectedly: {second}')
        if calls < 2:
            raise SystemExit('output callback was not invoked for both encodes')
        if not saw_same_priv:
            raise SystemExit('output callback priv was not preserved')
        print('output callback priv persistence verified')
    finally:
        sixel_output_unref(output)


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0153_python_api_output_callback_priv_persists_across_repeated_encode,
    ))
