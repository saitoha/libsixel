#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject DNL after the second scan.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0216_jpeg_dnl_late_reject" 1>&2 || {
    echo "not ok 1 - Reject DNL after the second scan"
    exit 0
}
echo "ok 1 - Reject DNL after the second scan"
exit 0
