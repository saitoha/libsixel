#!/bin/sh
# Test-plan: docs/testing/png-chrm-boundaries.md
# Verify external libpng withholds bad-length cHRM.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader SIXEL_LOADER_CMS_TARGET_COLORSPACE=linear \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0199_png_chrm_getter_reject 116 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP requested libpng or CMS engine is not compiled"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - external libpng withholds bad-length cHRM"
    exit 0
}
test "${trace#*"PNG_CHRM_GETTER|fixture=116|rejected=1"}" != "$trace" || {
    echo "not ok 1 - numeric observation did not complete"
    exit 0
}
echo "ok 1 - external libpng withholds bad-length cHRM"
exit 0
