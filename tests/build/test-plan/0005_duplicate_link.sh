#!/bin/sh
set -eux
# shellcheck source=tests/build/test-plan/fixture.sh
. "${0%/*}/fixture.sh"
printf '%s\n' '<!-- test-plan: @tests/_support/test-plans/one.list -->' \
    "[$member](../$member)" "[$member](../$member)" \
    '<!-- test-plan-end -->' > "$work/docs/plan.md"
reject 'duplicate inventory link'
