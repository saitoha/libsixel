#!/usr/bin/env python3
"""TAP test that encoder_encode rejects directory paths before C invocation."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encoder_encode rejects directory paths before C invocation'


def test_0074_python_api_encoder_encode_rejects_directory() -> None:
    try:
        from libsixel_wheel import sixel_encoder_encode
        from libsixel_wheel import sixel_encoder_new
        from libsixel_wheel import sixel_encoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    encoder = sixel_encoder_new()

    try:
        sixel_encoder_encode(encoder, '.')
    except RuntimeError as exc:
        sixel_encoder_unref(encoder)
        if 'directory' not in str(exc):
            raise AssertionError(f'unexpected error message: {exc}')
        print('encoder directory-path rejection verified')
        return

    sixel_encoder_unref(encoder)
    raise AssertionError('encoder accepted directory path')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0074_python_api_encoder_encode_rejects_directory,
    ))
