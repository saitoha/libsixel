#!/usr/bin/env python3
"""The 25th subsystem category requires reorganization."""
# Policy: docs/testing/guide.md

from fixture import Fixture

with Fixture() as fixture:
    for i in range(25):
        fixture.cases(1, f'owner-{i}/behavior')
    assert any('tests: 25 categories exceed 24' in error for error in fixture.errors())
