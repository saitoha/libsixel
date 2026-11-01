#!/usr/bin/env python3
"""TAP test that set_encoder_threads accepts uppercase ALL string input."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'set_encoder_threads accepts uppercase ALL string input'


def test_0087_python_api_set_encoder_threads_accepts_uppercase_all_string() -> None:
    try:
        from libsixel_wheel import sixel_set_encoder_threads
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    sixel_set_encoder_threads('ALL')
    print('set_encoder_threads uppercase all-string acceptance verified')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0087_python_api_set_encoder_threads_accepts_uppercase_all_string,
    ))
