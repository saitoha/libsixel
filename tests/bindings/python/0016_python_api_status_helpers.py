#!/usr/bin/env python3
"""TAP test for Python status helper APIs."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'status helper APIs classify success and failure correctly'
def test_0016_python_api_status_helpers() -> None:
    try:
        from libsixel_wheel import SIXEL_BAD_ALLOCATION
        from libsixel_wheel import SIXEL_FALSE
        from libsixel_wheel import SIXEL_RUNTIME_ERROR
        from libsixel_wheel import SIXEL_SUCCEEDED
        from libsixel_wheel import SIXEL_FAILED
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    statuses = [
        (SIXEL_FALSE, True),
        (SIXEL_RUNTIME_ERROR, False),
        (SIXEL_BAD_ALLOCATION, False),
    ]
    for status, expected_success in statuses:
        succeeded = SIXEL_SUCCEEDED(status)
        failed = SIXEL_FAILED(status)
        if succeeded == failed:
            raise SystemExit("helper functions returned contradictory states")
        if succeeded != expected_success:
            raise SystemExit("SIXEL_SUCCEEDED returned unexpected result")
        if failed == expected_success:
            raise SystemExit("SIXEL_FAILED returned unexpected result")

    print("status helper APIs verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0016_python_api_status_helpers))
