#!/usr/bin/env python3
"""An unnumbered shell case cannot escape the inventory."""
# Policy: docs/testing/guide.md

from fixture import Fixture

with Fixture() as fixture:
    fixture.add('owner/behavior/observation.t')
    assert any('NNNN_observation.ext' in error for error in fixture.errors())
