#!/usr/bin/env python3
"""Run optional analysis tests without importing plotting packages in C tests."""
import importlib.util
import sys
import unittest
from pathlib import Path

suite = unittest.TestSuite()
for path in sorted(Path(__file__).parent.glob("[0-9][0-9][0-9][0-9]_*.py")):
    spec = importlib.util.spec_from_file_location("test_" + path.stem, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    suite.addTests(unittest.defaultTestLoader.loadTestsFromModule(module))
result = unittest.TextTestRunner(verbosity=2).run(suite)
sys.exit(0 if result.wasSuccessful() else 1)
