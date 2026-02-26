#!/usr/bin/env python3
"""TAP test that module status helpers classify success and failure."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'module status helpers classify success and failure'
def test_0025_python_api_status_helpers_module() -> None:
    try:
        from libsixel_wheel import SIXEL_BAD_ALLOCATION
        from libsixel_wheel import SIXEL_FALSE
        from libsixel_wheel import SIXEL_RUNTIME_ERROR
        from libsixel_wheel import SIXEL_FAILED
        from libsixel_wheel import SIXEL_SUCCEEDED
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    statuses = [
        (SIXEL_FALSE, False),
        (SIXEL_RUNTIME_ERROR, False),
        (SIXEL_BAD_ALLOCATION, False),
    ]

    for status, expected_success in statuses:
        succeeded = SIXEL_SUCCEEDED(status)
        failed = SIXEL_FAILED(status)
        if succeeded == failed:
            raise SystemExit("status helpers returned contradictory states")
        if succeeded != expected_success:
            raise SystemExit("SIXEL_SUCCEEDED returned unexpected value")
        if failed == expected_success:
            raise SystemExit("SIXEL_FAILED returned unexpected value")

    print("module status helpers verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0025_python_api_status_helpers_module))
