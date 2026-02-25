#!/usr/bin/env python3
"""TAP test for uppercase auto string acceptance in sixel_set_threads()."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'set_threads accepts uppercase AUTO string input'


def test_0091_python_api_set_threads_accepts_uppercase_auto_string() -> None:
    try:
        from libsixel_wheel import sixel_set_threads
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    sixel_set_threads('AUTO')
    print('set_threads uppercase auto-string acceptance verified')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0091_python_api_set_threads_accepts_uppercase_auto_string,
    ))
