#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject missing amplitude bits before DNL.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0224_jpeg_dnl_amplitude_reject" 1>&2 || {
    echo "not ok 1 - Reject missing amplitude bits before DNL"
    exit 0
}
echo "ok 1 - Reject missing amplitude bits before DNL"
exit 0
