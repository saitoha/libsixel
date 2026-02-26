#!/usr/bin/env python3
"""TAP test for memoryview filename rejection in encoder.encode."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encoder encode rejects memoryview filename input'


def test_0126_python_api_encoder_encode_rejects_memoryview_filename() -> None:
    try:
        from libsixel_wheel import sixel_encoder_encode
        from libsixel_wheel import sixel_encoder_new
        from libsixel_wheel import sixel_encoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    encoder = sixel_encoder_new()

    try:
        sixel_encoder_encode(encoder, memoryview(b'dummy.png'))
    except TypeError:
        sixel_encoder_unref(encoder)
        print('encoder memoryview filename rejection verified')
        return

    sixel_encoder_unref(encoder)
    raise AssertionError('encoder accepted memoryview filename input')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0126_python_api_encoder_encode_rejects_memoryview_filename,
    ))
