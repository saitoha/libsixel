#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject absent DNL despite marker-shaped bytes in COM.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0211_jpeg_dnl_missing_reject" 1>&2 || {
    echo "not ok 1 - Reject absent DNL despite marker-shaped bytes in COM"
    exit 0
}
echo "ok 1 - Reject absent DNL despite marker-shaped bytes in COM"
exit 0
