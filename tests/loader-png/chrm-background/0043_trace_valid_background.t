#!/bin/sh
# Test-plan: docs/testing/png-chrm-boundaries.md
# Verify trace checker valid background using the actual source TAP.
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
    sh "${TOP_SRCDIR}/tests/loader-png/chrm-source-selection/0051_lcms2_16_file_bg_zero_icc_chrm_samples.t" 2>/dev/null) || result=$?
test "$result" -eq 0 || {
    echo "not ok 1 - source TAP did not exit normally"
    exit 0
}
# Compare only stdout; shell source tracing on stderr is not TAP evidence.
test "$trace" = "$plan
ok 1 - lcms2 16-file-bg-zero-icc-chrm original samples and coverage" || {
    echo "not ok 1 - trace checker valid background"
    exit 0
}
set -xv
echo "ok 1 - trace checker valid background"
exit 0
