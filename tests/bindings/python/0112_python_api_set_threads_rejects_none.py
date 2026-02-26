#!/usr/bin/env python3
"""TAP test for None rejection in set_threads."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'set_threads rejects None input'


def test_0115_python_api_set_threads_rejects_none() -> None:
    try:
        from libsixel_wheel import sixel_set_threads
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    try:
        sixel_set_threads(None)
    except ValueError:
        print('set_threads None rejection verified')
        return

    raise AssertionError('set_threads accepted None input')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0115_python_api_set_threads_rejects_none,
    ))
