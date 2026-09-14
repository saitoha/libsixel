#!/bin/sh
# Policy: docs/loader/coregraphics.md
# Verify explicit native icc rejects profile_arguments atomically.
set -eux
echo "1..1"
set -v

result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0188_colorsync_linear icc 4 colorsync \
    "${TOP_SRCDIR}/tests/data/color-numeric/display-p3.icc" \
    || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP CoreGraphics is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - explicit native icc rejects profile_arguments atomically"
    exit 0
}
echo "ok 1 - explicit native icc rejects profile_arguments atomically"
exit 0
