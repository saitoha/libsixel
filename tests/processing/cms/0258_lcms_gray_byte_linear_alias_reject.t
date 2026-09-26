#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Separate conversion succeeds; an expanding exact alias is rejected intact.
set -eux
result=0
trace=$(SIXEL_LOADER_CMS_RENDERING_INTENT=relative! SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cms/0258_lcms_gray_byte_linear_alias_reject 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP LCMS unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - 0258_lcms_gray_byte_linear_alias_reject separate values and unchanged rejection"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=apply|engine=lcms2|path=external|src=1|dst=4|ok=1"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - actual LCMS separate apply did not succeed"
    exit 0
}
case "${trace}" in *"LSXCMS1|intent_order=relative|exclusive=1"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - relative intent was not selected"
    exit 0
}
case "${trace}" in *"LCMSALIAS|src=1|gamma=0|count=521|separate-values-verified"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - independent separate values were not verified"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=reject|engine=lcms2|reason=expanding-alias|src=1|dst=4"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - specific alias rejection was not reported"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=apply|engine=lcms2|path=external|src=1|dst=4|ok=0"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - actual LCMS alias apply did not fail"
    exit 0
}
case "${trace}" in *"LCMSALIAS|src=1|gamma=0|counts=1,2,521|reject-bytes-verified"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - alias arenas were not preserved"
    exit 0
}
echo "ok 1 - 0258_lcms_gray_byte_linear_alias_reject separate values and unchanged rejection"
exit 0
