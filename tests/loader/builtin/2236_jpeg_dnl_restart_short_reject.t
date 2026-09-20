#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject an early DNL at a restart interval boundary.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0220_jpeg_dnl_restart_short_reject" 1>&2 || {
    echo "not ok 1 - Reject an early DNL at a restart interval boundary"
    exit 0
}
echo "ok 1 - Reject an early DNL at a restart interval boundary"
exit 0
