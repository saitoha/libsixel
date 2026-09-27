#!/bin/sh
# Policy: docs/loader/png-color-metadata.md
# builtin srgb over icc byte: numeric samples, storage, and alpha.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract \
    SIXEL_LOADER_CMS_TARGET_COLORSPACE=linear \
    SIXEL_LOADER_PREFER_8BIT=0 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cicp/0013_builtin_srgb_over_icc_byte 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required loader or CMS backend unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - builtin srgb over icc byte"
    exit 0
}
case "$trace" in
    *"LSXCMS2|event=apply|"*)
        echo "$trace" >&2
        echo "not ok 1 - redundant CMS apply"
        exit 0 ;;
esac
echo "ok 1 - builtin srgb over icc byte"
exit 0
