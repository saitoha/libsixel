#!/bin/sh
# Policy: docs/encoder/precision/README.md
# Verify libpng none CMS byte storage without a float image.
set -eux
echo "1..1"
set -v
result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 loader/0189_byte_storage \
    libpng none "${TOP_SRCDIR}/tests/data/inputs/formats/palette.png" \
    1302 0 1 || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP libpng unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - 0295_libpng_none_alpha_byte_storage"
    exit 0
}
echo "ok 1 - 0295_libpng_none_alpha_byte_storage"
exit 0
