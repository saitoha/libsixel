#!/bin/sh
# Test-plan: docs/testing/png-chrm-boundaries.md
# Verify trace checker none extra apply using the actual source TAP.
set -eux
plan=1..1
echo "$plan"
set -v

export SIXEL_CHRM_TRACE_RUNNER="${TEST_RUNNER_PATH}"
export SIXEL_CHRM_TRACE_RUNTIME="${SIXEL_RUNTIME-}"
# Nested negative TAP text must not enter the outer harness stream.
set +xv
result=0
trace=$(SIXEL_CHRM_TRACE_FAULT=valid SIXEL_RUNTIME=sh \
    TEST_RUNNER_PATH="${TOP_SRCDIR}/tests/_support/loader/libpng/chrm_trace_control.sh" \
    sh "${TOP_SRCDIR}/tests/loader-png/chrm-matrix/0037_none_8_rgb_zero_gama_chrm_samples.t" 2>/dev/null) || result=$?
test "$result" -eq 0 || {
    printf '\n%s\n' "not ok 1 - source TAP did not exit normally"
    exit 0
}
# Match the plan, blank line and verdict exactly on stdout.
test "$trace" = "$plan

not ok 1 - unexpected ICC apply remains" || {
    printf '\n%s\n' "not ok 1 - trace checker none extra apply"
    exit 0
}
set -xv
printf '\n%s\n' "ok 1 - trace checker none extra apply"
exit 0
