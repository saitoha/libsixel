#!/usr/bin/env python3
"""TAP smoke test for basic Python bindings import and lifecycle APIs."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'python bindings smoke test for import and lifecycle'


def test_0130_python_bindings_smoke() -> None:
    try:
        from libsixel_wheel import sixel_decoder_new
        from libsixel_wheel import sixel_decoder_unref
        from libsixel_wheel import sixel_encoder_new
        from libsixel_wheel import sixel_encoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    encoder = sixel_encoder_new()
    decoder = sixel_decoder_new()
    sixel_encoder_unref(encoder)
    sixel_decoder_unref(decoder)
    print('python bindings smoke lifecycle verified')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0130_python_bindings_smoke,
    ))
