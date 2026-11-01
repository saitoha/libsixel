#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify float black late infinity.
set -eux
result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0246_float_black_late_infinity \
    "${TOP_SRCDIR}/tests/data/color-numeric/tiff-float-black-le.tif" \
    "${TOP_SRCDIR}/tests/data/color-numeric/tiff-float-black-pinf.tif" || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP libtiff is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "not ok 1 - float black late infinity"
    exit 0
}
echo "ok 1 - float black late infinity"
exit 0
