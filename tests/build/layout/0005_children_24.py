#!/usr/bin/env python3
"""A parent may own 24 behavioral categories."""
# Policy: docs/testing/guide.md

from fixture import Fixture

with Fixture() as fixture:
    for i in range(24):
        fixture.cases(1, f'owner/behavior-{i}')
    assert fixture.errors() == []
