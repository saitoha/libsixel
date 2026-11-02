#!/bin/sh
# Policy: docs/build/test-harness.md
set -eux
# shellcheck source=tests/build/discovery/fixture.sh
. "${0%/*}/fixture.sh"
add sample/existing/0001_existing.t
fail_scan
expect '' 'TESTS='
