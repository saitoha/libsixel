#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/jpeg.md
# Preserve twelve-bit extended sequential samples with DNL.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0205_jpeg_dnl_12bit_numeric" 1>&2 || {
    echo "not ok 1 - Preserve twelve-bit extended sequential samples with DNL"
    exit 0
}
echo "ok 1 - Preserve twelve-bit extended sequential samples with DNL"
exit 0
