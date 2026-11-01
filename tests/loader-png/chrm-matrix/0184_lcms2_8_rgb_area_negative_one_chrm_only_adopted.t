#!/bin/sh
# Test-plan: docs/testing/png-chrm-boundaries.md
# Verify lcms2 8-rgb-area-negative-one-chrm-only matrix adoption and finite frame.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader SIXEL_LOADER_CMS_TARGET_COLORSPACE=linear \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0202_png_chrm_accept 233 lcms2 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP requested libpng or CMS engine is not compiled"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - lcms2 8-rgb-area-negative-one-chrm-only matrix adoption and finite frame"
    exit 0
}
case "${trace}" in *"PNG_CHRM_ACCEPT|fixture=233|engine=lcms2|checked=3|"*) true ;; *) false ;; esac || {
    echo "not ok 1 - numeric observation did not complete"
    exit 0
}
# Consume every apply in order; partial or extra records must fail.
case "${trace}" in *"LSXCMS2|event=apply"*) false ;; *) true ;; esac || {
    echo "not ok 1 - unexpected ICC apply remains"
    exit 0
}
echo "ok 1 - lcms2 8-rgb-area-negative-one-chrm-only matrix adoption and finite frame"
exit 0
