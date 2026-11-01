#!/usr/bin/env python3
"""Two category levels are accepted."""
# Policy: docs/testing/guide.md

from fixture import Fixture

with Fixture() as fixture:
    fixture.cases(1)
    assert fixture.errors() == []
