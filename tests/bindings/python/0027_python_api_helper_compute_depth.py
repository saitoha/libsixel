#!/usr/bin/env python3
"""TAP test that sixel_helper_compute_depth returns positive depth for RGB888."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'sixel_helper_compute_depth returns positive depth for RGB888'
def test_0027_python_api_helper_compute_depth() -> None:
    try:
        from libsixel_wheel import SIXEL_PIXELFORMAT_RGB888
        from libsixel_wheel import sixel_helper_compute_depth
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    depth = sixel_helper_compute_depth(SIXEL_PIXELFORMAT_RGB888)
    if depth <= 0:
        raise SystemExit(f"unexpected depth value: {depth}")

    print(f"sixel_helper_compute_depth verified (depth={depth})")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0027_python_api_helper_compute_depth))
