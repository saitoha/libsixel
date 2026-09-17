#!/bin/sh
# Policy: docs/loader/libjpeg.md
# Expand original Gray16 lossless samples without native color conversion.
set -eux
result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" jpeg-cmyk/gray16-source \
    "${TOP_SRCDIR}" || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP libjpeg 16-bit API is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "not ok 1 - original Gray16 samples expand to exact RGB float values"
    exit 0
}
echo "ok 1 - original Gray16 samples expand to exact RGB float values"
exit 0
