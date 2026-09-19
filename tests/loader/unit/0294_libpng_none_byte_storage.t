#!/bin/sh
# Policy: docs/functionality/precision.md
# Verify libpng none CMS byte storage without a float image.
set -eux
echo "1..1"
set -v
result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/0189_byte_storage \
    libpng none "${TOP_SRCDIR}/tests/data/inputs/formats/snake-32.png" \
    1024 0 1 || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP libpng unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - 0294_libpng_none_byte_storage"
    exit 0
}
echo "ok 1 - 0294_libpng_none_byte_storage"
exit 0
