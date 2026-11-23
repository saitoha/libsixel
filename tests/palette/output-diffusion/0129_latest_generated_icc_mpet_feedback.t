#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Reuse palette 1 with relative D2B1/B2D1 matf elements in physical PCS.
set -eux
echo '1..1'
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/latest_feedback \
    "icc:P${TOP_SRCDIR}/tests/data/color-numeric/register-feedback-mpet.icc" \
    >/dev/null || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP asynchronous palette scheduling unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - latest generated mpet ICC palette and FS pixels'
    exit 0
}
echo 'ok 1 - latest generated mpet ICC palette and FS pixels'
exit 0
