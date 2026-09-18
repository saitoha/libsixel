#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify the actual float transform backend and apply result.
set -eux
echo "1..1"
set -v

trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "cms/0017_builtin_byte_to_float" 2>&1) || {
    echo "not ok 1 - actual builtin float transform failed"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=apply|engine=builtin|path=extended-rgb|src=2|dst=4|ok=1"*) true ;; *) false ;; esac || {
    echo "not ok 1 - actual builtin float apply trace missing"
    exit 0
}
echo "ok 1 - actual builtin float apply trace"
exit 0
