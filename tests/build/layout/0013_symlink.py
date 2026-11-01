#!/usr/bin/env python3
"""A symbolic link cannot masquerade as a source case."""
# Policy: docs/testing/guide.md

from fixture import Fixture

with Fixture() as fixture:
    target = fixture.add('_support/target')
    (fixture.root / 'owner/behavior').mkdir(parents=True)
    (fixture.root / 'owner/behavior/0001_link.t').symlink_to(target)
    assert any('regular source file' in error for error in fixture.errors())
