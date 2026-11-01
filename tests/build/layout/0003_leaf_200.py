#!/usr/bin/env python3
"""The leaf capacity includes its 200th case."""
# Policy: docs/testing/guide.md

from fixture import Fixture

with Fixture() as fixture:
    fixture.cases(200)
    assert fixture.errors() == []
