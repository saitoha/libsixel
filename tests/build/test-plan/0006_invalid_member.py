#!/usr/bin/env python3
"""Validate one explicit inventory boundary."""
from fixture import Fixture

with Fixture() as f:
    f.write('tests/_support/test-plans/one.list', 'tests/owner/behavior/*.t\n')
    assert any('invalid test path' in e for e in f.errors())
