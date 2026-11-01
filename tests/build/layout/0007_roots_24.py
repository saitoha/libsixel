#!/usr/bin/env python3
"""The test root may own 24 subsystem categories."""
# Policy: docs/testing/guide.md

from fixture import Fixture

with Fixture() as fixture:
    for i in range(24):
        fixture.cases(1, f'owner-{i}/behavior')
    assert fixture.errors() == []
