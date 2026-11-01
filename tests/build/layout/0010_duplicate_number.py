#!/usr/bin/env python3
"""Different observations cannot share a case number."""
# Policy: docs/testing/guide.md

from fixture import Fixture

with Fixture() as fixture:
    fixture.cases(1)
    fixture.add('owner/behavior/0001_different.t')
    assert any('unique' in error for error in fixture.errors())
