#!/bin/sh
# Policy: docs/encoder/precision/README.md
# Verify libpng builtin CMS byte storage without a float image.
set -eux
echo "1..1"
set -v
result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/0189_byte_storage \
    libpng builtin "${TOP_SRCDIR}/tests/data/inputs/formats/snake-32.png" \
    1024 0 1 || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP libpng unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - 0297_libpng_builtin_untagged_byte_storage"
    exit 0
}
echo "ok 1 - 0297_libpng_builtin_untagged_byte_storage"
exit 0
