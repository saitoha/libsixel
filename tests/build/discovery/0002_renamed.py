#!/usr/bin/env python3
"""A renamed source test replaces its old name without configure."""
# Policy: docs/build/test-harness.md

import sys
from fixture import Fixture

with Fixture(sys.argv[1]) as fixture:
    old = fixture.add("sample/existing/0001_old.t")
    assert fixture.run() == ["sample/existing/0001_old.t"]
    old.rename(old.with_name("0001_renamed.t"))
    assert fixture.run() == ["sample/existing/0001_renamed.t"]
