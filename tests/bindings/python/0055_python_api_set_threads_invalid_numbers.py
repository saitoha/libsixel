#!/usr/bin/env python3
"""TAP test that set_threads rejects zero and negative numeric inputs."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'set_threads rejects zero and negative numeric inputs'
def test_0055_python_api_set_threads_invalid_numbers() -> None:
    try:
        from libsixel_wheel import sixel_set_threads
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    rejected_zero = False
    rejected_negative = False

    try:
        sixel_set_threads(0)
    except ValueError:
        rejected_zero = True

    try:
        sixel_set_threads(-1)
    except ValueError:
        rejected_negative = True

    if not rejected_zero or not rejected_negative:
        raise SystemExit("set_threads accepted non-positive numeric input")

    print("set_threads numeric validation paths verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0055_python_api_set_threads_invalid_numbers))
