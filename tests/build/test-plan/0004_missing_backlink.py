#!/usr/bin/env python3
"""Validate one explicit inventory boundary."""
from fixture import Fixture

with Fixture() as f:
    f.write('references/test-plan-links', '')
    assert any('missing Test-plan reference' in e for e in f.errors())
