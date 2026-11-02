#!/bin/sh
# Policy: docs/testing/guide.md
set -eux
# shellcheck source=tests/build/layout/fixture.sh
. "${0%/*}/fixture.sh"
add owner/behavior/0001_first.t
add owner/behavior/0001_second.t
reject 'unique and contiguous'
