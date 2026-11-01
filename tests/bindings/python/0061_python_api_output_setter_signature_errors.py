#!/usr/bin/env python3
"""TAP test that output setters reject missing required value argument."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'output setters reject missing required value argument'
def test_0064_python_api_output_setter_signature_errors() -> None:
    try:
        from libsixel_wheel import sixel_output_new
        from libsixel_wheel import sixel_output_set_skip_header
        from libsixel_wheel import sixel_output_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    output = sixel_output_new(lambda _data, _priv: None)
    rejected = False
    try:
        sixel_output_set_skip_header(output)
    except TypeError:
        rejected = True
    sixel_output_unref(output)

    if not rejected:
        raise SystemExit('output setter unexpectedly accepted missing argument')

    print('output setter signature validation verified')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(DESCRIPTION,
                                           test_0064_python_api_output_setter_signature_errors))
