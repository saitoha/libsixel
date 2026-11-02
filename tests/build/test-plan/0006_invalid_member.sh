#!/bin/sh
set -eux
# shellcheck source=tests/build/test-plan/fixture.sh
. "${0%/*}/fixture.sh"
printf '%s\n' 'tests/owner/behavior/*.t' > "$work/tests/_support/test-plans/one.list"
reject 'invalid test path'
