#!/usr/bin/env python3
"""TAP test for auto thread byte input acceptance."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'set_threads accepts auto keyword as byte input'


def test_0079_python_api_set_threads_accepts_auto_bytes() -> None:
    try:
        from libsixel_wheel import sixel_set_threads
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    sixel_set_threads(b'auto')
    print('set_threads auto-byte acceptance verified')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0079_python_api_set_threads_accepts_auto_bytes,
    ))
