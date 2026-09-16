#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify float early failure control.
set -eux
result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0248_float_early_failure_control \
    "${TOP_SRCDIR}/tests/data/color-numeric/tiff-float-rgb-le.tif" \
    "${TOP_SRCDIR}/tests/data/color-numeric/tiff-float-rgb-nan.tif" || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP libtiff is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "not ok 1 - float early failure control"
    exit 0
}
echo "ok 1 - float early failure control"
exit 0
