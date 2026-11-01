#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject every truncated DNL marker field.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0214_jpeg_dnl_truncated_reject" 1>&2 || {
    echo "not ok 1 - Reject every truncated DNL marker field"
    exit 0
}
echo "ok 1 - Reject every truncated DNL marker field"
exit 0
