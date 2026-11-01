#!/usr/bin/env python3
"""A partial discovery failure stops make instead of accepting a suite."""
# Policy: docs/build/test-harness.md

import sys
from fixture import Fixture

with Fixture(sys.argv[1]) as fixture:
    fixture.add("0001_existing.t")
    fixture.fail_scan()
    fixture.run(success=False)
