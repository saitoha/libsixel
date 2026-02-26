#!/usr/bin/env python3
"""TAP test that helper compute_depth rejects non-integer pixelformat."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'helper compute_depth rejects non-integer pixelformat'
def test_0061_python_api_helper_compute_depth_invalid_type() -> None:
    try:
        from libsixel_wheel import sixel_helper_compute_depth
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    rejected = False
    try:
        sixel_helper_compute_depth(object())
    except (TypeError, RuntimeError):
        rejected = True
    except Exception as exc:  # ctypes may wrap TypeError as ArgumentError
        if exc.__class__.__name__ == "ArgumentError":
            rejected = True
        else:
            raise

    if not rejected:
        raise SystemExit('compute_depth accepted non-convertible pixelformat object')

    print('compute_depth type validation verified')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(DESCRIPTION,
                                           test_0061_python_api_helper_compute_depth_invalid_type))
