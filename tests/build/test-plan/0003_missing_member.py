#!/usr/bin/env python3
"""Validate one explicit inventory boundary."""
from fixture import Fixture

with Fixture() as f:
    (f.root / f.member).unlink()
    assert any('missing scoped test' in e for e in f.errors())
