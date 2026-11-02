#!/bin/sh
# Independent scope, document-link and source-reference inputs.
src_root=$1
work=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-plan-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
member=tests/owner/behavior/0001_observation.t
mkdir -p "$work/tests/owner/behavior" "$work/tests/_support/test-plans" "$work/docs" "$work/references"
: > "$work/$member"
printf '%s\n' "$member" > "$work/tests/_support/test-plans/one.list"
printf '%s\n' '<!-- test-plan: @tests/_support/test-plans/one.list -->' \
    "[$member](../$member)" '<!-- test-plan-end -->' > "$work/docs/plan.md"
printf 'docs/plan.md|%s\n' "$member" > "$work/references/test-plan-links"
: > "$work/references/test-plan-errors"
accept() { sh "$src_root/tests/_static/sh/check-test-plan-inventory.sh" "$work" "$work/references"; }
reject() {
    result=0
    errors=$(accept 2>&1) || result=$?
    test "$result" -ne 0 || return 1
    case "$errors" in *"$1"*) return 0 ;; *) printf '%s\n' "$errors" >&2; return 1 ;; esac
}
