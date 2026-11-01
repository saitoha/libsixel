#!/bin/sh
# Policy: docs/loader/libjpeg.md
# Verify gray16 cms none with independent source values.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    jpeg-gray/0016_gray16_cms_none 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required libjpeg precision API or CMS is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - gray16 cms none values"
    exit 0
}
case "$trace" in *"LSXCMS2|event=apply|"*)
    echo "$trace" >&2
    echo "not ok 1 - unexpected profile application"
    exit 0 ;;
esac
echo "ok 1 - gray16 cms none"
exit 0
