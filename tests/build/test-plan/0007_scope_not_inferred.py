#!/usr/bin/env python3
"""Validate one explicit inventory boundary."""
from fixture import Fixture

with Fixture() as f:
    f.write('tests/_support/test-plans/one.list', f.member + '\ntests/owner/behavior/0002_extra.t\n')
    f.write('tests/owner/behavior/0002_extra.t', '')
    assert any('missing inventory link: tests/owner/behavior/0002_extra.t' in e for e in f.errors())
