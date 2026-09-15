#!/bin/sh
# Verify result classification through the production staticcheck dispatcher.
# Policy: docs/testing/staticcheck.md
set -eu

echo '1..1'
src_root=$1
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-tap-dispatch-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM
# shellcheck source=tests/_static/sh/tap-case.sh
. "$src_root/tests/_static/sh/tap-case.sh"
index=0
total=0
pass_count=0
skip_count=0
fail_count=0
expected_pass=0
expected_skip=0
expected_fail=0

# Literal independent streams include malformed protocol and shell diagnostics
# that resemble TAP. Each is consumed by the same function used by the suite.
while IFS='|' read -r sample_name expected child_status tap_input stderr_input; do
    # The quoted script is interpreted by the child shell.
    # shellcheck disable=SC2016
    if run_case_tap "$sample_name" sh -c \
            'printf "%b" "$1"; printf "%b" "$2" >&2; test "$3" != signal || kill -TERM "$$"; exit "$3"' \
            sh "$tap_input" "$stderr_input" "$child_status" \
            >"$tmpdir/control"; then
        control_status=0
    else
        control_status=$?
    fi
    control_output=$(cat "$tmpdir/control")
    case "$expected:$control_status:$control_output" in
        pass:0:*"PASS $sample_name"*) expected_pass=$((expected_pass + 1)) ;;
        skip:0:*"SKIP $sample_name"*) expected_skip=$((expected_skip + 1)) ;;
        fail:1:*"FAIL $sample_name"*) expected_fail=$((expected_fail + 1)) ;;
        *)
            echo 'not ok 1 - staticcheck TAP classification'
            printf '# case %s expected %s, status %s\n' \
                "$sample_name" "$expected" "$control_status"
            printf '%s\n' "$control_output"
            exit 0
            ;;
    esac
    case "$sample_name:$control_output" in
        failure-diagnostics:*"not ok 1 - broken"*"sentinel stderr"*) ;;
        failure-diagnostics:*)
            echo 'not ok 1 - staticcheck discarded failure diagnostics'
            exit 0
            ;;
    esac
done <<'CASES'
pass|pass|0|1..1\nok 1 - ready\n|
trailing-plan|pass|0|ok 1\nok 2\n1..2\n|
implicit-number|pass|0|1..3\nok\nok 2\nok - final\n|
version|pass|0|TAP version 13\n1..1\nok 1\n|
crlf|pass|0|1..1\r\nok 1\r\n|
no-final-newline|pass|0|1..1\nok 1|
comments|pass|0|# not ok 50\n1..1\n# Bail out! diagnostic\nok 1\n|
stderr-tap|pass|0|1..1\nok 1\n|not ok 2\n1..500\nBail out! verbose source\n
mixed-pass-skip|pass|0|1..2\nok 1 # SKIP absent\nok 2\n|
all-skip|skip|0|1..2\nok 1 # skip absent\nok 2 # SKIP absent\n|
empty-skip|skip|0|1..0 # SKIP unavailable\n|
failure|fail|0|1..1\nnot ok 1\n|
failure-diagnostics|fail|0|1..1\nnot ok 1 - broken\n# expected value\n|sentinel stderr\n
mixed-fail-skip|fail|0|1..2\nok 1 # SKIP absent\nnot ok 2\n|
child-failure|fail|2|1..1\nok 1\n|
child-skip-failure|fail|77|1..0 # SKIP absent\n|
child-abnormal|fail|signal|1..1\nok 1\n|
empty-output|fail|0||
stderr-only|fail|0||1..1\nok 1\n
missing-plan|fail|0|ok 1\n|
missing-result|fail|0|1..1\n|
missing-last|fail|0|1..2\nok 1\n|
extra-result|fail|0|1..1\nok 1\nok 2\n|
duplicate-number|fail|0|1..2\nok 1\nok 1\n|
wrong-number|fail|0|1..1\nok 2\n|
zero-number|fail|0|1..1\nok 0\n|
malformed-number|fail|0|1..1\nok 1x\n|
duplicate-plan|fail|0|1..1\n1..1\nok 1\n|
mid-plan|fail|0|ok 1\n1..2\nok 2\n|
malformed-plan|fail|0|1..x\nok 1\n|
wrong-plan-start|fail|0|2..2\nok 1\n|
unbounded-plan|fail|0|1..99999999999999999999\nok 1\n|
empty-unmarked-plan|fail|0|1..0\n|
result-after-skip|fail|0|1..0 # SKIP absent\nok 1\n|
bailout|fail|0|1..1\nBail out! broken prerequisite\n|
late-bailout|fail|0|1..1\nok 1\nBail out! teardown failed\n|
malformed-result|fail|0|1..1\nok1\n|
stdout-source|fail|0|1..1\necho "not ok 1"\nok 1\n|
late-version|fail|0|1..1\nTAP version 13\nok 1\n|
todo|fail|0|1..1\nnot ok 1 # TODO pending invariant\n|
yaml|pass|0|1..1\nok 1\n  ---\n  message: not ok 2\n  ...\n|
unterminated-yaml|fail|0|1..1\nok 1\n  ---\n  message: broken\n|
CASES

test "$pass_count:$skip_count:$fail_count" = \
    "$expected_pass:$expected_skip:$expected_fail" || {
    echo 'not ok 1 - staticcheck TAP summary counts differ'
    exit 0
}
echo 'ok 1 - staticcheck TAP classification and failure diagnostics'
