#!/usr/bin/env python3
"""A third category level is rejected."""
# Policy: docs/testing/guide.md

from fixture import Fixture

with Fixture() as fixture:
    fixture.cases(1, 'owner/behavior/extra')
    assert any('two category levels' in error for error in fixture.errors())
