#!/bin/sh
# Policy: docs/build/test-harness.md
set -eux
# shellcheck source=tests/build/discovery/fixture.sh
. "${0%/*}/fixture.sh"
add sample/existing/0001_removed.t
expect sample/existing/0001_removed.t
: > "$work/0001_removed.t"
rm "$source_root/sample/existing/0001_removed.t"
expect ''
