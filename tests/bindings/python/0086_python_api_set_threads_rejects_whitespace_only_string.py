#!/usr/bin/env python3
"""TAP test that set_threads rejects whitespace-only string input."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'set_threads rejects whitespace-only string input'


def test_0086_python_api_set_threads_rejects_whitespace_only_string() -> None:
    try:
        from libsixel_wheel import sixel_set_threads
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    try:
        sixel_set_threads('   ')
    except ValueError:
        print('set_threads whitespace-only-string rejection verified')
        return

    raise SystemExit('set_threads accepted whitespace-only string input')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0086_python_api_set_threads_rejects_whitespace_only_string,
    ))
