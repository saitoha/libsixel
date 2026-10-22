#!/bin/sh
# Verify single-pass totals, timing records and byte-preserving diagnostics.
# Policy: docs/build/test-harness.md
set -eux
echo '1..1'
set -v

src_root=$1
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-suite-report-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM
# The shell must copy diagnostics as data, including a
# filename containing spaces, an apostrophe and a literal dollar sign.
bad_name="failure ' \$dollar"
printf ':test-result: PASS first\r\n:test-result: SKIP optional\r\n:global-test-result: PASS\r\n:duration-ms: 11.000\r\n:test-name: pass.t\r\n:copy-in-global-log: no\r\n' > "$tmpdir/pass.trs"
cat > "$tmpdir/expected.trs" <<'EOF'
:test-result: XFAIL expected
:global-test-result: XFAIL
:duration-ms: 2.000
:test-name: expected.t
:copy-in-global-log: NO
EOF
cat > "$tmpdir/$bad_name.trs" <<'EOF'
:test-result: FAIL broken
:test-result: XPASS unexpected
:test-result: ERROR infrastructure
:global-test-result: FAIL
:duration-ms: 20.000
:subtest-time: 4.000 ms PASS batch/inner.t
:test-name: failed.t
EOF
printf 'diagnostic\r\nunterminated' > "$tmpdir/$bad_name.log"
printf ':test-result: SKIP absent\n:global-test-result: SKIP\n:test-name: absent.t\n:copy-in-global-log: no' > "$tmpdir/absent.trs"
printf '%s\n' "$tmpdir/pass.trs" "$tmpdir/expected.trs" \
    "$tmpdir/$bad_name.trs" "$tmpdir/absent.trs" > "$tmpdir/manifest"
set -- sh "$src_root/build-aux/test-suite-report.sh" "${2:-awk}" \
    "$tmpdir/aggregate" -v suite_title=fixture -v quiet=1 \
    -v builddir="$tmpdir" -v counts_file="$tmpdir/counts" \
    -v failures_file="$tmpdir/failures"
"$@" "$tmpdir/manifest" > "$tmpdir/report" 2> "$tmpdir/stderr" || {
    echo 'not ok 1 - suite report could not consume valid records'
    exit 0
}
{
    # Match the shell collector's native cat semantics. GNV may expose an
    # incomplete RMS text record with a terminator; POSIX cat keeps it absent.
    printf 'fixture\n\nFAIL: %s\n' "$bad_name"
    cat "$tmpdir/$bad_name.log"
    printf '\n'
    cat <<'EOF'
TOTAL: 7
PASS: 1
SKIP: 2
XFAIL: 1
FAIL: 1
XPASS: 1
ERROR: 1
Test durations (wall time, including process startup)
====================================================

test-time: 11.000 ms PASS pass.t
test-time: 2.000 ms XFAIL expected.t
test-time: 4.000 ms PASS batch/inner.t
test-time: 20.000 ms BATCH failed.t
test-time: unmeasured ms SKIP absent.t

Slow tests: 0; slow setup/batch processes: 0 (default 30 ms; explicit per-test exception budgets).
EOF
} > "$tmpdir/expected-report"
cmp "$tmpdir/expected-report" "$tmpdir/report" &&
test "$(cat "$tmpdir/counts")" = '7 1 2 1 1 1 1' &&
test "$(cat "$tmpdir/failures")" = "FAIL: $bad_name" &&
test -s "$tmpdir/stderr" || {
    echo 'not ok 1 - suite totals, timings or diagnostic bytes changed'
    exit 0
}
# A requested diagnostic that is missing adds an infrastructure error; it
# does not erase the original result or prevent the remaining summary.
cat > "$tmpdir/missing-log.trs" <<'EOF'
:test-result: PASS original
:global-test-result: PASS
:duration-ms: 1.000
EOF
printf '%s\n' "$tmpdir/missing-log.trs" > "$tmpdir/manifest"
"$@" "$tmpdir/manifest" > "$tmpdir/report" 2> "$tmpdir/stderr" || {
    echo 'not ok 1 - missing diagnostics prevented result aggregation'
    exit 0
}
test "$(cat "$tmpdir/counts")" = '1 1 0 0 0 0 1' || {
    echo 'not ok 1 - missing diagnostics did not add an error'
    exit 0
}
awk '/^missing log file:/ { found = 1 } END { exit !found }' "$tmpdir/report" || {
    echo 'not ok 1 - missing requested diagnostics were not counted'
    exit 0
}
# Missing metadata or unusable timing must abort report publication.
printf '%s\n' "$tmpdir/missing.trs" > "$tmpdir/manifest"
"$@" "$tmpdir/manifest" > "$tmpdir/report" 2> "$tmpdir/stderr" && {
    echo 'not ok 1 - unreadable result metadata was accepted'
    exit 0
}
for duration in unmeasured invalid; do
    printf ':test-result: PASS case\n:global-test-result: PASS\n:duration-ms: %s\n:copy-in-global-log: no\n' "$duration" > "$tmpdir/invalid.trs"
    printf '%s\n' "$tmpdir/invalid.trs" > "$tmpdir/manifest"
    "$@" "$tmpdir/manifest" > "$tmpdir/report" 2> "$tmpdir/stderr" && {
        echo 'not ok 1 - invalid required timing was accepted'
        exit 0
    }
done
: > "$tmpdir/manifest"
"$@" "$tmpdir/manifest" > "$tmpdir/report" 2> "$tmpdir/stderr" &&
test "$(cat "$tmpdir/counts")" = '0 0 0 0 0 0 0' || {
    echo 'not ok 1 - an empty selection did not produce zero counts'
    exit 0
}
"$@" -v counts_file="$tmpdir/missing/counts" "$tmpdir/manifest" \
    > "$tmpdir/report" 2> "$tmpdir/stderr" && {
    echo 'not ok 1 - an unwritable shell handoff was accepted'
    exit 0
}
echo 'ok 1 - single-pass suite aggregation preserves result and timing semantics'
exit 0
