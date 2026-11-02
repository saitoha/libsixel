#!/bin/sh
# Policy: docs/testing/guide.md
set -eux
# shellcheck source=tests/build/layout/fixture.sh
. "${0%/*}/fixture.sh"
add 'owner/behavior/deeper/0001_case.t'
reject 'two category levels'
