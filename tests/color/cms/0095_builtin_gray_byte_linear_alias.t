#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify builtin device values and the specified buffer layout.
set -eux
echo "1..1"
set -v
result=0
trace=$(SIXEL_LOADER_CMS_RENDERING_INTENT=relative! SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cms/0255_builtin_gray_byte_linear_alias 2>&1) || result=$?
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - 0255_builtin_gray_byte_linear_alias values and buffer integrity"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=apply|engine=builtin|path=extended-rgb|src=1|dst=4|ok=1"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - builtin analytic transform did not apply"
    exit 0
}
case "${trace}" in *"LSXCMS1|intent_order=relative|exclusive=1"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - relative intent was not selected"
    exit 0
}
case "${trace}" in *"BUILTINDEVICE|gray=1|gamma=0|alias=1|count=521|values-and-guards-verified"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - independent values and layout were not checked"
    exit 0
}
echo "ok 1 - 0255_builtin_gray_byte_linear_alias values and buffer integrity"
exit 0
