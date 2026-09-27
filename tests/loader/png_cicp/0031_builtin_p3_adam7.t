#!/bin/sh
# Policy: docs/loader/png-color-metadata.md
# builtin p3 adam7: numeric samples, storage, and alpha.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract \
    SIXEL_LOADER_CMS_TARGET_COLORSPACE=linear \
    SIXEL_LOADER_PREFER_8BIT=0 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cicp/0031_builtin_p3_adam7 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required loader or CMS backend unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - builtin p3 adam7"
    exit 0
}
case "$trace" in
    *"LSXCMS2|event=apply|engine=builtin|"*"|ok=1"*) ;;
    *) echo "$trace" >&2; echo "not ok 1 - CMS apply missing"; exit 0 ;;
esac
echo "ok 1 - builtin p3 adam7"
exit 0
