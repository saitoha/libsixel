#!/bin/sh
# Policy: docs/loader/builtin/gif.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify GIF ICC cms none byte samples and loader behavior.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract \
    SIXEL_LOADER_CMS_TARGET_COLORSPACE=linear \
    SIXEL_BACKGROUND_POLICY=file_first \
    SIXEL_LOADER_PREFER_8BIT=0 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gifc/0008_gif_icc_cms_none_byte_numeric 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP requested CMS backend unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - GIF ICC cms none byte"
    exit 0
}
case "$trace" in
    *"LSXCMS2|event=apply|"*) echo "not ok 1 - unexpected CMS apply"; exit 0 ;;
esac
echo "ok 1 - GIF ICC cms none byte"
exit 0
