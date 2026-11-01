#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject dac destination.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0097_jp_dac_destination_reject" 1>&2 || {
    echo "not ok 1 - Reject dac destination"
    exit 0
}
echo "ok 1 - Reject dac destination"
exit 0
