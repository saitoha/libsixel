#!/bin/sh
# Test-plan: docs/testing/png-chrm-boundaries.md
# Verify collinear 1 permutation 5 exact signed area.
set -eux
echo "1..1"
set -v

result=0
trace=$(SIXEL_TRACE_TOPIC=loader SIXEL_LOADER_CMS_TARGET_COLORSPACE=linear \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0201_png_chrm_area 11 2>&1) || result=$?
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - collinear 1 permutation 5 exact signed area"
    exit 0
}
test "${trace#*"PNG_CHRM_AREA|sample=11|checked=2"}" != "$trace" || {
    echo "not ok 1 - numeric observation did not complete"
    exit 0
}
echo "ok 1 - collinear 1 permutation 5 exact signed area"
exit 0
