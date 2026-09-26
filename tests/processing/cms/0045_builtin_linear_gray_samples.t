#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify original replicated gray samples through builtin linear output.
set -eux
echo "1..1"
set -v

result=0
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    cms/0045_linear_gray_samples builtin 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP requested CMS engine is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - builtin original gray samples to linear RGB"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=apply|engine=builtin|path=extended-rgb|src=9|dst=4|ok=1"*) true ;; *) false ;; esac || {
    echo "not ok 1 - requested CMS did not apply to packed gray float"
    exit 0
}
echo "ok 1 - builtin original gray samples to linear RGB"
exit 0
