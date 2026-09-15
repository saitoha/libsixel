#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify a public float frame after an actual LCMS device-unit transform.
set -eux
result=0
trace=$(SIXEL_LOADER_CMS_RENDERING_INTENT=relative! SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/0172_lcms_lab_frame "${TOP_SRCDIR}/tests/data/colormgmt/input/custom/lab_high_precision_icc.tiff" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required loader or LCMS2 unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - 0172_lcms_lab_frame public frame values"
    exit 0
}
test "${trace#*"LSXCMS2|event=apply|engine=lcms2|path=external|src=8|dst=4|ok=1"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - expected LCMS apply did not execute"
    exit 0
}
test "${trace#*"LCMSFRAME|mode=3|values-verified"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - independent public frame values were not checked"
    exit 0
}
echo "ok 1 - 0172_lcms_lab_frame public frame values"
exit 0
