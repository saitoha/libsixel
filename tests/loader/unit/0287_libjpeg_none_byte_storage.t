#!/bin/sh
# Policy: docs/encoder/precision/README.md
# Verify libjpeg none CMS byte storage without a float image.
set -eux
echo "1..1"
set -v
result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/0189_byte_storage \
    libjpeg none "${TOP_SRCDIR}/tests/data/inputs/formats/snake-jpeg-8bit-rgb-seq444.jpg" \
    4096 0 1 || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP libjpeg unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - 0287_libjpeg_none_byte_storage"
    exit 0
}
echo "ok 1 - 0287_libjpeg_none_byte_storage"
exit 0
