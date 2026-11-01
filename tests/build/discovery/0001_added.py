#!/usr/bin/env python3
"""An added source test appears on the next make invocation."""
# Policy: docs/build/test-harness.md

import sys
from fixture import Fixture

with Fixture(sys.argv[1]) as fixture:
    fixture.add("0001_existing.t")
    assert fixture.run() == ["0001_existing.t"]
    fixture.add("nested/0001_added.t")
    assert fixture.run() == ["0001_existing.t", "nested/0001_added.t"]
