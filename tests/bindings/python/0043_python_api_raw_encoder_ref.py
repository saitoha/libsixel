#!/usr/bin/env python3
"""TAP test for raw encoder ref/unref APIs in libsixel.__init__.py."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'raw encoder ref and unref APIs are callable'
def test_0043_python_api_raw_encoder_ref() -> None:
    try:
        from libsixel_wheel import sixel_encoder_new
        from libsixel_wheel import sixel_encoder_ref
        from libsixel_wheel import sixel_encoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    encoder = sixel_encoder_new()
    sixel_encoder_ref(encoder)
    sixel_encoder_unref(encoder)
    sixel_encoder_unref(encoder)

    print("raw encoder ref/unref verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0043_python_api_raw_encoder_ref))
