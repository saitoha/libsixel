#!/usr/bin/env python3
"""A removed source test is absent even if a build copy remains."""
# Policy: docs/build/test-harness.md

import sys
from fixture import Fixture

with Fixture(sys.argv[1]) as fixture:
    old = fixture.add("0001_removed.t")
    assert fixture.run() == ["0001_removed.t"]
    (fixture.work / old.name).touch()
    old.unlink()
    assert fixture.run() == []
