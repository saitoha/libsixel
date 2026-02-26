#!/usr/bin/env python3
"""TAP test that sixel_helper_format_error returns a non-empty message."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'sixel_helper_format_error returns a non-empty message'
def test_0026_python_api_helper_format_error() -> None:
    try:
        from libsixel_wheel import SIXEL_RUNTIME_ERROR
        from libsixel_wheel import sixel_helper_format_error
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    message = sixel_helper_format_error(SIXEL_RUNTIME_ERROR)
    if not message:
        raise SystemExit("sixel_helper_format_error returned an empty message")

    print("sixel_helper_format_error verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0026_python_api_helper_format_error))
