#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject a DNL that still declares zero height.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0212_jpeg_dnl_zero_reject" 1>&2 || {
    echo "not ok 1 - Reject a DNL that still declares zero height"
    exit 0
}
echo "ok 1 - Reject a DNL that still declares zero height"
exit 0
