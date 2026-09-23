#!/bin/sh
# Policy: docs/loader/libtiff.md
# Verify icclab8 separate none against source and value contracts.
set -eux
result=0
trace=$(SIXEL_LOADER_CMS_RENDERING_INTENT=relative SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0525_device_icclab8_separate_none 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required loader or CMS engine is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - icclab8 separate none"
    exit 0
}
case "$trace" in *"LSXCMS2|event=apply|"*) false ;; *) true ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - unmanaged or incompatible input reached CMS apply"
    exit 0
}
echo "ok 1 - icclab8 separate none"
exit 0
