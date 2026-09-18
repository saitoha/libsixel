#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify normalized device inputs with independent ICC precision intervals.
set -eux
result=0
trace=$(SIXEL_LOADER_CMS_RENDERING_INTENT=relative! SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cms/0245_lcms_lab_gamma_alias 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP LCMS2 unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - 0245_lcms_lab_gamma_alias input units and buffer integrity"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=apply|engine=lcms2|path=external|src=8|dst=4|ok=1"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - LCMS transform did not apply"
    exit 0
}
case "${trace}" in *"LSXCMS1|intent_order=relative|exclusive=1"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - relative intent was not selected"
    exit 0
}
case "${trace}" in *"values-and-guards-verified"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - independent values were not checked"
    exit 0
}
echo "ok 1 - 0245_lcms_lab_gamma_alias input units and buffer integrity"
exit 0
