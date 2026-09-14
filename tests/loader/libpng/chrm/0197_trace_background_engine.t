#!/bin/sh
# Test-plan: docs/testing/png-chrm-boundaries.md
# Verify trace checker background engine using the actual source TAP.
set -eux
plan=1..1
echo "$plan"
set -v

export SIXEL_CHRM_TRACE_RUNNER="${TEST_RUNNER_PATH}"
export SIXEL_CHRM_TRACE_RUNTIME="${SIXEL_RUNTIME-}"
# Nested negative TAP text must not enter the outer harness stream.
set +xv
result=0
trace=$(SIXEL_CHRM_TRACE_FAULT=wrong-background-engine SIXEL_RUNTIME=sh \
    TEST_RUNNER_PATH="${TOP_SRCDIR}/tests/loader/libpng/chrm_trace_control.sh" \
    sh "${TOP_SRCDIR}/tests/loader/libpng/chrm/0171_lcms2_16_file_bg_zero_icc_chrm_samples.t" 2>/dev/null) || result=$?
test "$result" -eq 0 || {
    echo "not ok 1 - source TAP did not exit normally"
    exit 0
}
# Compare only stdout; shell source tracing on stderr is not TAP evidence.
test "$trace" = "$plan
not ok 1 - ICC apply 2 fields differ" || {
    echo "not ok 1 - trace checker background engine"
    exit 0
}
set -xv
echo "ok 1 - trace checker background engine"
exit 0
