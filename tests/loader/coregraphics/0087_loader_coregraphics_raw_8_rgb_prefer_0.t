#!/bin/sh
# Policy: docs/loader/coregraphics.md
# Verify raw 8 rgb original samples with float preference 0.
set -eux
echo "1..1"
set -v

result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0187_coregraphics_png raw 8 rgb 0 0 2 colorsync \
    "${TOP_SRCDIR}/tests/data/color-numeric/png-raw-8-rgb.png" \
    || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP CoreGraphics is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - raw 8 rgb original samples with float preference 0"
    exit 0
}
echo "ok 1 - raw 8 rgb original samples with float preference 0"
exit 0
