#!/usr/bin/env python3
"""The 201st case requires a category split."""
# Policy: docs/testing/guide.md

from fixture import Fixture

with Fixture() as fixture:
    fixture.cases(201)
    assert any('201 cases exceed 200' in error for error in fixture.errors())
