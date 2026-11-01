#!/usr/bin/env python3
"""TAP test that output setter APIs accept expected arguments."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'output setter APIs accept expected arguments'
def test_0038_python_api_output_setters() -> None:
    try:
        from libsixel_wheel import SIXEL_ENCODEPOLICY_FAST
        from libsixel_wheel import SIXEL_PALETTETYPE_RGB
        from libsixel_wheel import sixel_output_new
        from libsixel_wheel import sixel_output_set_8bit_availability
        from libsixel_wheel import sixel_output_set_encode_policy
        from libsixel_wheel import sixel_output_set_gri_arg_limit
        from libsixel_wheel import sixel_output_set_ormode
        from libsixel_wheel import sixel_output_set_palette_type
        from libsixel_wheel import sixel_output_set_penetrate_multiplexer
        from libsixel_wheel import sixel_output_set_skip_dcs_envelope
        from libsixel_wheel import sixel_output_set_skip_header
        from libsixel_wheel import sixel_output_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    output = sixel_output_new(lambda _data, _priv: None)
    sixel_output_set_8bit_availability(output, 1)
    sixel_output_set_gri_arg_limit(output, 1)
    sixel_output_set_penetrate_multiplexer(output, 1)
    sixel_output_set_skip_dcs_envelope(output, 1)
    sixel_output_set_skip_header(output, 1)
    sixel_output_set_palette_type(output, SIXEL_PALETTETYPE_RGB)
    sixel_output_set_ormode(output, 1)
    sixel_output_set_encode_policy(output, SIXEL_ENCODEPOLICY_FAST)
    sixel_output_unref(output)

    print("output setter APIs verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0038_python_api_output_setters))
