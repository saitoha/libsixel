#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify logluv scanline size failure.
set -eux
result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0220_logluv_scanline_size_failure \
    "${TOP_SRCDIR}/tests/data/color-numeric/tiff-logluv32.tif" || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required loader or CMS engine is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "not ok 1 - logluv scanline size failure"
    exit 0
}
echo "ok 1 - logluv scanline size failure"
exit 0
