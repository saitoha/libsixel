#!/usr/bin/env python3
"""Installed and static cases count even without runtime registration."""
# Policy: docs/testing/guide.md

from fixture import Fixture

with Fixture() as fixture:
    fixture.cases(198)
    fixture.add('owner/behavior/0199_static.py')
    fixture.add('owner/behavior/0200_installed.ps1')
    fixture.add('owner/behavior/0201_source.sh')
    assert any('201 cases exceed 200' in error for error in fixture.errors())
