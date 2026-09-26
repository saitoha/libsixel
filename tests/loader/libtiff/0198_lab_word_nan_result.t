#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify lab word nan result.
set -eux
result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0198_lab_word_nan_result \
    "${TOP_SRCDIR}/tests/data/color-numeric/tiff-lab-word-521.tif" || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required loader or CMS engine is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "not ok 1 - lab word nan result"
    exit 0
}
echo "ok 1 - lab word nan result"
exit 0
