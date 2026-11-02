#!/bin/sh
set -eux
# shellcheck source=tests/build/test-plan/fixture.sh
. "${0%/*}/fixture.sh"
: > "$work/references/test-plan-links"
reject 'missing Test-plan reference'
