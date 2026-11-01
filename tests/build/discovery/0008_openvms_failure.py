#!/usr/bin/env python3
"""A partial scan failure stops the line-oriented worker prerequisite."""
# Policy: docs/build/test-harness.md

import sys
from fixture import Fixture

with Fixture(sys.argv[1], openvms=True) as fixture:
    fixture.add("sample/existing/0001_existing.t")
    fixture.fail_scan()
    fixture.run(success=False)
