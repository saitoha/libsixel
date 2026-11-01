#!/bin/sh
# Policy: docs/loader/libjpeg.md
# Verify independent source-derived values: 12bit-cmyk-seq444 prefix failure.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    jpeg-cmyk/prefix-failure "${TOP_SRCDIR}" 1 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required libjpeg precision API or CMS is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - 12bit-cmyk-seq444 prefix failure"
    exit 0
}
echo "ok 1 - 12bit-cmyk-seq444 prefix failure"
exit 0
