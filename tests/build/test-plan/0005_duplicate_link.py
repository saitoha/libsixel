#!/usr/bin/env python3
"""Validate one explicit inventory boundary."""
from fixture import Fixture

with Fixture() as f:
    p = f.root / 'docs/plan.md'
    p.write_text(p.read_text().replace('<!-- test-plan-end -->', f'[{f.member}](../{f.member})\n<!-- test-plan-end -->'))
    assert any('duplicate inventory link' in e for e in f.errors())
