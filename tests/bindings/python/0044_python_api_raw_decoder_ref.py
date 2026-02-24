#!/usr/bin/env python3
"""TAP test for raw decoder ref/unref APIs in libsixel.__init__.py."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'raw decoder ref and unref APIs are callable'
def test_0044_python_api_raw_decoder_ref() -> None:
    try:
        from libsixel_wheel import sixel_decoder_new
        from libsixel_wheel import sixel_decoder_ref
        from libsixel_wheel import sixel_decoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    decoder = sixel_decoder_new()
    sixel_decoder_ref(decoder)
    sixel_decoder_unref(decoder)
    sixel_decoder_unref(decoder)

    print("raw decoder ref/unref verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0044_python_api_raw_decoder_ref))
