#!/bin/sh
# Policy: docs/encoder/precision/README.md
# Verify libjpeg none CMS float preference.
set -eux
echo "1..1"
set -v
result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/0189_byte_storage \
    libjpeg none "${TOP_SRCDIR}/tests/data/inputs/formats/snake-jpeg-8bit-rgb-seq444.jpg" \
    4096 1 0 || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP libjpeg unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - 0289_libjpeg_none_float_preference"
    exit 0
}
echo "ok 1 - 0289_libjpeg_none_float_preference"
exit 0
