#!/bin/sh
# Policy: docs/functionality/precision.md
# Verify libwebp none CMS float preference.
set -eux
echo "1..1"
set -v
result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/0189_byte_storage \
    libwebp none "${TOP_SRCDIR}/tests/data/inputs/snake_64.webp" \
    4096 1 0 || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP libwebp unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - 0292_libwebp_none_float_preference"
    exit 0
}
echo "ok 1 - 0292_libwebp_none_float_preference"
exit 0
