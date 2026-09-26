#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify float late read failure.
set -eux
result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0249_float_late_read_failure \
    "${TOP_SRCDIR}/tests/data/color-numeric/tiff-float-rgb-le.tif" \
    "${TOP_SRCDIR}/tests/data/color-numeric/tiff-float-rgb-le.tif" || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP libtiff is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "not ok 1 - float late read failure"
    exit 0
}
echo "ok 1 - float late read failure"
exit 0
