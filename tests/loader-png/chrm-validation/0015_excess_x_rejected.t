#!/bin/sh
# Test-plan: docs/testing/png-chrm-boundaries.md
# Verify excess x is rejected before unsafe conversion.
set -eux
echo "1..1"
set -v

result=0
trace=$(SIXEL_TRACE_TOPIC=loader SIXEL_LOADER_CMS_TARGET_COLORSPACE=linear \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0197_png_chrm_reject 5 2>&1) || result=$?
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - excess x is rejected before unsafe conversion"
    exit 0
}
case "${trace}" in *"PNG_CHRM_GRID|reject|sample=5|checked=1"*) true ;; *) false ;; esac || {
    echo "not ok 1 - numeric observation did not complete"
    exit 0
}
echo "ok 1 - excess x is rejected before unsafe conversion"
exit 0
