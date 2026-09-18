#!/bin/sh
# Policy: docs/loader/color-management.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify a public float frame after an actual LCMS device-unit transform.
set -eux
result=0
trace=$(SIXEL_LOADER_CMS_RENDERING_INTENT=relative! SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/0196_lcms_cmyk32_missing "${TOP_SRCDIR}/tests/data/color-numeric/lcms-cmyk32-missing.psd" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required loader or LCMS2 unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - 0196_lcms_cmyk32_missing public frame values"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=apply|engine=lcms2|path=external|src=7|dst=4|ok=1"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - expected LCMS apply did not execute"
    exit 0
}
case "${trace}" in *"LCMSFRAME|mode=1|values-verified"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - independent public frame values were not checked"
    exit 0
}
echo "ok 1 - 0196_lcms_cmyk32_missing public frame values"
exit 0
