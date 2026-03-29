#!/usr/bin/env python3
"""TAP test that setopt-related constants stay synchronized with the header."""

from __future__ import annotations

import os
import pathlib
import re

from _taptest import run_embedded_tap_test


DESCRIPTION = 'setopt constants stay synchronized with header definitions'


def test_0169_python_api_constants_setopt_sync() -> None:
    try:
        import libsixel_wheel as wheel
        from libsixel_wheel import SIXEL_LOADER_OPTION_START_FRAME_NO
        from libsixel_wheel import SIXEL_LUT_POLICY_MAHALANOBIS
        from libsixel_wheel import SIXEL_LUT_POLICY_RBC
        from libsixel_wheel import sixel_loader_new
        from libsixel_wheel import sixel_loader_setopt
        from libsixel_wheel import sixel_loader_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    header = pathlib.Path(os.path.expandvars("${TOP_SRCDIR}/include/sixel.h.in"))
    expected_prefixes = (
        "SIXEL_LOADER_OPTION_",
        "SIXEL_LUT_POLICY_",
        "SIXEL_COLORSPACE_",
    )
    expected = {
        name
        for name in re.findall(r"^#define\s+(SIXEL_[A-Z0-9_]+)\s+", header.read_text(encoding="utf-8"), re.M)
        if name.startswith(expected_prefixes)
    }
    missing = sorted(name for name in expected if not hasattr(wheel, name))
    if missing:
        raise SystemExit("missing setopt constants: " + ", ".join(missing))

    if SIXEL_LUT_POLICY_RBC != 0x9:
        raise SystemExit("SIXEL_LUT_POLICY_RBC value mismatch")
    if SIXEL_LUT_POLICY_MAHALANOBIS != 0xa:
        raise SystemExit("SIXEL_LUT_POLICY_MAHALANOBIS value mismatch")

    loader = sixel_loader_new()
    sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_START_FRAME_NO, "0")
    sixel_loader_unref(loader)

    print("setopt constants and start-frame loader option are synchronized")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0169_python_api_constants_setopt_sync))
