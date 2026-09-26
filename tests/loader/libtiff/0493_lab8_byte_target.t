#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify lab8 byte target with independent values.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0493_lab8_byte_target 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required loader or CMS engine is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - lab8 byte target"
    exit 0
}
case "$trace" in *"LSXCMS2|event=apply|"*) false ;; *) true ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - unexpected CMS apply"
    exit 0
}
echo "ok 1 - lab8 byte target"
exit 0
