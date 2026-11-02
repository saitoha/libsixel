#!/bin/sh
# Policy: docs/build/test-harness.md
set -eux
# shellcheck source=tests/build/discovery/fixture.sh
. "${0%/*}/fixture.sh"
add sample/existing/0001_existing.t
expect sample/existing/0001_existing.t
add sample/added/0001_added.t
expect 'sample/added/0001_added.t
sample/existing/0001_existing.t'
