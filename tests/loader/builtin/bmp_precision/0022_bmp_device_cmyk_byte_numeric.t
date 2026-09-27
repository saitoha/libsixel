#!/bin/sh
# Policy: docs/loader/builtin/bmp.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify BMP device cmyk byte.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract \
    SIXEL_LOADER_CMS_TARGET_COLORSPACE=linear \
    SIXEL_LOADER_CMS_RENDERING_INTENT='relative!' \
    SIXEL_BACKGROUND_POLICY=explicit_first \
    SIXEL_LOADER_PREFER_8BIT=0 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" bmpc/0022_bmp_device_cmyk_byte_numeric 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP requested CMS backend unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - BMP device cmyk byte"
    exit 0
}
case "$trace" in
    *"LSXCMS2|event=apply|"*)
        echo "$trace" >&2
        echo "not ok 1 - unexpected CMS apply"
        exit 0 ;;
esac
echo "ok 1 - BMP device cmyk byte"
exit 0
