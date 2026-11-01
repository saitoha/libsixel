#!/usr/bin/env python3
"""The line-oriented inventory refreshes before the next worker run."""
# Policy: docs/build/test-harness.md

import sys
from fixture import Fixture

with Fixture(sys.argv[1], openvms=True) as fixture:
    old = fixture.add("0001_old.t")
    assert fixture.run() == ["0001_old.t"]
    old.rename(old.with_name("0001_renamed.t"))
    assert fixture.run() == ["0001_renamed.t"]
