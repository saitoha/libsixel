#!/usr/bin/env python3
"""TAP test that set_encoder_threads accepts whitespace-padded all bytes input."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'set_encoder_threads accepts whitespace-padded all bytes input'


def test_0079_python_api_set_encoder_threads_accepts_whitespace_all_bytes() -> None:
    try:
        from libsixel_wheel import sixel_set_encoder_threads
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    sixel_set_encoder_threads(b'  all  ')
    print('set_encoder_threads whitespace all-byte acceptance verified')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0079_python_api_set_encoder_threads_accepts_whitespace_all_bytes,
    ))
