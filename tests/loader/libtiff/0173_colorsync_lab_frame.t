#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify a public float frame after an actual ColorSync device-unit transform.
set -eux
result=0
trace=$(SIXEL_LOADER_CMS_RENDERING_INTENT=relative! SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/0173_colorsync_lab_frame "${TOP_SRCDIR}/tests/data/colormgmt/input/custom/lab_high_precision_icc.tiff" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required loader or ColorSync unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - 0173_colorsync_lab_frame public frame values"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=apply|engine=colorsync|path=external|src=8|dst=4|ok=1"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - expected ColorSync apply did not execute"
    exit 0
}
case "${trace}" in *"LSXCMS1|intent_order=relative|exclusive=1"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - relative intent was not selected"
    exit 0
}
case "${trace}" in *"CSLABFRAME|values-verified"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - independent public frame values were not checked"
    exit 0
}
echo "ok 1 - 0173_colorsync_lab_frame public frame values"
exit 0
