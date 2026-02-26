#!/usr/bin/env python3
"""TAP test that encoder encode rejects bytes directory filename input."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encoder encode rejects bytes directory filename input'


def test_0127_python_api_encoder_encode_rejects_directory_bytes_filename() -> None:
    try:
        from libsixel_wheel import sixel_encoder_encode
        from libsixel_wheel import sixel_encoder_new
        from libsixel_wheel import sixel_encoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    encoder = sixel_encoder_new()

    try:
        sixel_encoder_encode(encoder, b'.')
    except RuntimeError:
        sixel_encoder_unref(encoder)
        print('encoder bytes directory filename rejection verified')
        return

    sixel_encoder_unref(encoder)
    raise AssertionError('encoder accepted bytes directory filename input')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0127_python_api_encoder_encode_rejects_directory_bytes_filename,
    ))
