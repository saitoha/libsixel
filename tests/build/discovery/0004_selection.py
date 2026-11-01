#!/usr/bin/env python3
"""An explicit TESTS selection takes precedence over discovery."""
# Policy: docs/build/test-harness.md

import sys
from fixture import Fixture

with Fixture(sys.argv[1]) as fixture:
    fixture.add("0001_existing.t")
    fixture.fail_scan()
    assert fixture.run("TESTS=chosen.t") == ["chosen.t"]
