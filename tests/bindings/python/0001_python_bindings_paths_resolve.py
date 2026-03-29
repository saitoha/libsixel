#!/usr/bin/env python3
"""TAP test that packaged Python binding paths resolve to bundled artifacts."""

from __future__ import annotations

from pathlib import Path

from _taptest import run_embedded_tap_test


DESCRIPTION = 'packaged python binding paths are configured'


def test_0001_python_bindings_paths_resolve() -> None:
    try:
        import libsixel_wheel as wheel
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    module_root = Path(wheel.__file__).resolve().parent
    libs_dir = module_root / "_libs"
    if not libs_dir.is_dir():
        raise SystemExit("wheel package does not include _libs directory")

    candidates = (
        list(libs_dir.glob("*.so"))
        + list(libs_dir.glob("*.so.*"))
        + list(libs_dir.glob("*.dylib"))
        + list(libs_dir.glob("*.dll"))
    )
    if not candidates:
        raise SystemExit("wheel package does not include bundled shared library")

    print("packaged python binding and shared library paths resolved")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0001_python_bindings_paths_resolve))
