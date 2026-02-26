#!/usr/bin/env python3
"""TAP test that output 8bit availability getter is callable."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'output 8bit availability getter is callable'
def test_0045_python_api_output_get_8bit() -> None:
    try:
        from libsixel_wheel import sixel_output_get_8bit_availability
        from libsixel_wheel import sixel_output_new
        from libsixel_wheel import sixel_output_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    output = sixel_output_new(lambda _data, _priv: None)
    sixel_output_get_8bit_availability(output)
    sixel_output_unref(output)

    print("output 8bit getter verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0045_python_api_output_get_8bit))
