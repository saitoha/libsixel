#!/usr/bin/env python3
"""TAP test that encoder encode rejects non-pathlike filename objects."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encoder encode rejects non-pathlike filename object'


def test_0136_python_api_encoder_encode_rejects_nonpathlike_filename() -> None:
    try:
        from libsixel_wheel import sixel_encoder_encode
        from libsixel_wheel import sixel_encoder_new
        from libsixel_wheel import sixel_encoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    class SymbolLike:
        pass

    encoder = sixel_encoder_new()
    try:
        sixel_encoder_encode(encoder, SymbolLike())
    except (RuntimeError, ValueError, TypeError):
        sixel_encoder_unref(encoder)
        print('encoder non-pathlike filename rejection verified')
        return

    sixel_encoder_unref(encoder)
    raise SystemExit('encoder accepted non-pathlike filename object')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0136_python_api_encoder_encode_rejects_nonpathlike_filename,
    ))
