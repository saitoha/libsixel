#!/usr/bin/env python3
"""TAP test that output get_8bit returns a valid availability state."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'output get_8bit returns a valid availability state'
def test_0068_python_api_output_get_8bit_return_value() -> None:
    try:
        from libsixel_wheel import sixel_output_get_8bit_availability
        from libsixel_wheel import sixel_output_new
        from libsixel_wheel import sixel_output_set_8bit_availability
        from libsixel_wheel import sixel_output_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    output = sixel_output_new(lambda _data, _priv: None)
    sixel_output_set_8bit_availability(output, 1)
    result = sixel_output_get_8bit_availability(output)
    sixel_output_unref(output)

    if result not in (0, 1):
        raise SystemExit(f'get_8bit returned unexpected value: {result}')

    print(f'output get_8bit return value verified ({result})')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(DESCRIPTION,
                                           test_0068_python_api_output_get_8bit_return_value))
