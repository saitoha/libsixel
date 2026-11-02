#!/bin/sh
set -eux
# shellcheck source=tests/build/test-plan/fixture.sh
. "${0%/*}/fixture.sh"
printf '%s\n' "$member" "$member" > "$work/tests/_support/test-plans/one.list"
reject 'duplicate test path'
