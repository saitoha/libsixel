#!/usr/bin/env python3
"""Reserved support and fixture files do not consume case capacity."""
# Policy: docs/testing/guide.md

from fixture import Fixture

with Fixture() as fixture:
    fixture.cases(200)
    fixture.add('owner/behavior/0001_observation.c')
    fixture.add('_support/old/deep/tree/0001_fixture.t')
    fixture.add('data/0001_specimen.py')
    assert fixture.errors() == []
