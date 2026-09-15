#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify normalized device inputs with independent ICC precision intervals.
set -eux
result=0
trace=$(SIXEL_LOADER_CMS_RENDERING_INTENT=relative! SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cms/0238_lcms_cmyk_linear 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP LCMS2 unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - 0238_lcms_cmyk_linear input units and buffer integrity"
    exit 0
}
test "${trace#*"LSXCMS2|event=apply|engine=lcms2|path=external|src=7|dst=4|ok=1"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - LCMS transform did not apply"
    exit 0
}
test "${trace#*"LSXCMS1|intent_order=relative|exclusive=1"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - relative intent was not selected"
    exit 0
}
test "${trace#*"values-and-guards-verified"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - independent values were not checked"
    exit 0
}
echo "ok 1 - 0238_lcms_cmyk_linear input units and buffer integrity"
exit 0
