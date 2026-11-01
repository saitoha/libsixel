#!/usr/bin/env python3
"""An explicitly empty TESTS selection does not fall back to discovery."""
# Policy: docs/build/test-harness.md

import sys
from fixture import Fixture

with Fixture(sys.argv[1]) as fixture:
    fixture.add("sample/existing/0001_existing.t")
    fixture.fail_scan()
    assert fixture.run("TESTS=") == []
