#!/usr/bin/env python3
"""Case numbers cannot contain gaps."""
# Policy: docs/testing/guide.md

from fixture import Fixture

with Fixture() as fixture:
    fixture.add('owner/behavior/0002_observation.t')
    assert any('contiguous' in error for error in fixture.errors())
