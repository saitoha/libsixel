#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify builtin device values and the specified buffer layout.
set -eux
echo "1..1"
set -v
result=0
trace=$(SIXEL_LOADER_CMS_RENDERING_INTENT=relative! SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cms/0252_builtin_cmyk_float_gamma 2>&1) || result=$?
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - 0252_builtin_cmyk_float_gamma values and buffer integrity"
    exit 0
}
test "${trace#*"LSXCMS2|event=apply|engine=builtin|path=extended-rgb|src=7|dst=4|ok=1"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - builtin analytic transform did not apply"
    exit 0
}
test "${trace#*"LSXCMS1|intent_order=relative|exclusive=1"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - relative intent was not selected"
    exit 0
}
test "${trace#*"BUILTINDEVICE|gray=0|gamma=1|alias=0|count=521|values-and-guards-verified"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - independent values and layout were not checked"
    exit 0
}
echo "ok 1 - 0252_builtin_cmyk_float_gamma values and buffer integrity"
exit 0
