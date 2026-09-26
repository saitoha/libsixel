#!/bin/sh
# Policy: docs/encoder/precision/README.md
# Verify libtiff builtin CMS byte storage without a float image.
set -eux
echo "1..1"
set -v
result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/0189_byte_storage \
    libtiff builtin "${TOP_SRCDIR}/tests/data/color-numeric/tiff-byte-rgb-all256-none.tif" \
    256 0 1 || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP libtiff unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - 0286_libtiff_builtin_byte_storage"
    exit 0
}
echo "ok 1 - 0286_libtiff_builtin_byte_storage"
exit 0
