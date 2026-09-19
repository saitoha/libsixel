#!/bin/sh
# Policy: docs/functionality/precision.md
# Verify libwebp none CMS byte storage without a float image.
set -eux
echo "1..1"
set -v
result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/0189_byte_storage \
    libwebp none "${TOP_SRCDIR}/tests/data/inputs/formats/webp-lossless-rgb64-cwebp-transform-subsample.webp" \
    4096 0 1 || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP libwebp unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - 0298_libwebp_none_lossless_byte_storage"
    exit 0
}
echo "ok 1 - 0298_libwebp_none_lossless_byte_storage"
exit 0
