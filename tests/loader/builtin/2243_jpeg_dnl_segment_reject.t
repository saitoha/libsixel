#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject an oversized segment while searching for DNL.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0227_jpeg_dnl_segment_reject" 1>&2 || {
    echo "not ok 1 - Reject an oversized segment while searching for DNL"
    exit 0
}
echo "ok 1 - Reject an oversized segment while searching for DNL"
exit 0
