#!/usr/bin/env python3
"""TAP test that output new/ref/unref lifecycle APIs are callable."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'output new/ref/unref lifecycle APIs are callable'
def test_0037_python_api_output_lifecycle() -> None:
    try:
        from libsixel_wheel import sixel_output_new
        from libsixel_wheel import sixel_output_ref
        from libsixel_wheel import sixel_output_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    chunks = []

    def _write(data: bytes, _priv: object) -> None:
        chunks.append(data)

    output = sixel_output_new(_write)
    sixel_output_ref(output)
    sixel_output_unref(output)
    sixel_output_unref(output)

    print("output lifecycle APIs verified")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0037_python_api_output_lifecycle))
