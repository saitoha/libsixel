#!/usr/bin/env python3
"""TAP test migrated from shell wrapper to Python-only execution."""

from __future__ import annotations

import os

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encodes image via wheel'
ARGV = [os.path.expandvars("${ARTIFACT_LOCAL_DIR}")]
def test_0001_python_bindings() -> None:
    import ctypes.util
    import glob
    import os
    import pathlib
    import sys


    def _prefer_build_library(name, original_find):
        libdir = os.environ.get("LIBSIXEL_LIBDIR")
        if libdir:
            prefixes = ["lib", ""]
            suffixes = [".so", ".dylib", ".dll"]
            for prefix in prefixes:
                for suffix in suffixes:
                    pattern = os.path.join(libdir, f"{prefix}{name}*{suffix}")
                    matches = sorted(glob.glob(pattern))
                    if matches:
                        return matches[0]
        return original_find(name)


    ctypes.util.find_library = (
        lambda name, _orig=ctypes.util.find_library: _prefer_build_library(name, _orig)
    )

    try:
        from libsixel_wheel import SIXEL_PIXELFORMAT_RGB888
        from libsixel_wheel.encoder import Encoder, SIXEL_OPTFLAG_OUTPUT

        root = pathlib.Path(sys.argv[1])
        output = root / "sample.six"

        pixels = bytes([
            255, 0, 0,
            0, 255, 0,
            0, 0, 255,
            255, 255, 255,
        ])

        encoder = Encoder()
        encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(output))
        for _ in range(2):
            encoder.encode_bytes(pixels, 2, 2, SIXEL_PIXELFORMAT_RGB888, None)

        if not output.exists() or output.stat().st_size == 0:
            raise SystemExit("missing or empty sixel output")

        print("encode succeeded")
    except OSError as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, ARGV, test_0001_python_bindings))
