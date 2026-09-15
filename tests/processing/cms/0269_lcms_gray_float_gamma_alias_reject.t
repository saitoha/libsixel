#!/bin/sh
# Policy: docs/loader/color-management.md
# Separate conversion succeeds; an expanding exact alias is rejected intact.
set -eux
result=0
trace=$(SIXEL_LOADER_CMS_RENDERING_INTENT=relative! SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cms/0269_lcms_gray_float_gamma_alias_reject 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP LCMS unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - 0269_lcms_gray_float_gamma_alias_reject separate values and unchanged rejection"
    exit 0
}
test "${trace#*"LSXCMS2|event=apply|engine=lcms2|path=external|src=9|dst=4|ok=1"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - actual LCMS separate apply did not succeed"
    exit 0
}
test "${trace#*"LSXCMS1|intent_order=relative|exclusive=1"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - relative intent was not selected"
    exit 0
}
test "${trace#*"LCMSALIAS|src=9|gamma=1|count=521|separate-values-verified"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - independent separate values were not verified"
    exit 0
}
test "${trace#*"LSXCMS2|event=reject|engine=lcms2|reason=expanding-alias|src=9|dst=4"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - specific alias rejection was not reported"
    exit 0
}
test "${trace#*"LSXCMS2|event=apply|engine=lcms2|path=external|src=9|dst=4|ok=0"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - actual LCMS alias apply did not fail"
    exit 0
}
test "${trace#*"LCMSALIAS|src=9|gamma=1|counts=1,2,521|reject-bytes-verified"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - alias arenas were not preserved"
    exit 0
}
echo "ok 1 - 0269_lcms_gray_float_gamma_alias_reject separate values and unchanged rejection"
exit 0
