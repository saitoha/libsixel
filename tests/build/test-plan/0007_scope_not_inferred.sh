#!/bin/sh
set -eux
# shellcheck source=tests/build/test-plan/fixture.sh
. "${0%/*}/fixture.sh"
printf '%s\n' "$member" 'tests/owner/behavior/0002_extra.t' > "$work/tests/_support/test-plans/one.list"
: > "$work/tests/owner/behavior/0002_extra.t"
reject 'missing inventory link: tests/owner/behavior/0002_extra.t'
