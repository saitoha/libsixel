#!/usr/bin/env python3
"""TAP test for sixel_set_threads() in libsixel.__init__.py."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'set_threads accepts valid inputs and rejects invalid input'
def test_0035_python_api_set_threads() -> None:
    try:
        from libsixel_wheel import sixel_set_threads
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    sixel_set_threads(1)
    sixel_set_threads("auto")

    rejected = False
    try:
        sixel_set_threads("invalid")
    except ValueError:
        rejected = True

    if not rejected:
        raise SystemExit("sixel_set_threads accepted invalid input")

    print("set_threads verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0035_python_api_set_threads))
