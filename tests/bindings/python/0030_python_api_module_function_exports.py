#!/usr/bin/env python3
"""TAP test for exported function coverage in python/libsixel/__init__.py."""

from __future__ import annotations

import ast
import os
import pathlib

from _taptest import run_embedded_tap_test


DESCRIPTION = 'all public functions in __init__.py are exported by libsixel_wheel'
def test_0030_python_api_module_function_exports() -> None:
    try:
        import libsixel_wheel as module
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    source_path = pathlib.Path(
        os.path.expandvars("${TOP_SRCDIR}/python/libsixel/__init__.py")
    )
    tree = ast.parse(source_path.read_text(encoding="utf-8"))

    expected = []
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and not node.name.startswith("_"):
            expected.append(node.name)

    missing = []
    noncallable = []
    for name in expected:
        if not hasattr(module, name):
            missing.append(name)
            continue
        attr = getattr(module, name)
        if not callable(attr):
            noncallable.append(name)

    if missing:
        raise SystemExit("missing exported functions: " + ", ".join(sorted(missing)))
    if noncallable:
        raise SystemExit("non-callable exports: " + ", ".join(sorted(noncallable)))

    print(f"verified {len(expected)} exported functions")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0030_python_api_module_function_exports))
