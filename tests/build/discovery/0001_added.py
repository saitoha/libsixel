#!/usr/bin/env python3
"""An added source test appears on the next make invocation."""
# Policy: docs/build/test-harness.md

import sys
from fixture import Fixture

with Fixture(sys.argv[1]) as fixture:
    fixture.add("sample/existing/0001_existing.t")
    assert fixture.run() == ["sample/existing/0001_existing.t"]
    fixture.add("sample/added/0001_added.t")
    assert fixture.run() == ["sample/added/0001_added.t", "sample/existing/0001_existing.t"]
