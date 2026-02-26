#!/usr/bin/env python3
"""TAP test for missing bytes path rejection in encoder encode."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encoder encode rejects missing bytes path input'


def test_0098_python_api_encoder_encode_rejects_missing_bytes_path() -> None:
    try:
        from libsixel_wheel import sixel_encoder_encode
        from libsixel_wheel import sixel_encoder_new
        from libsixel_wheel import sixel_encoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    encoder = sixel_encoder_new()

    try:
        sixel_encoder_encode(encoder, b'/path/which/does/not/exist.png')
    except RuntimeError:
        sixel_encoder_unref(encoder)
        print('encoder missing bytes path rejection verified')
        return

    sixel_encoder_unref(encoder)
    raise AssertionError('encoder accepted missing bytes path')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0098_python_api_encoder_encode_rejects_missing_bytes_path,
    ))
