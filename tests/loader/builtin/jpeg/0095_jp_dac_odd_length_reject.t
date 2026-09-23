#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject dac odd length.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0095_jp_dac_odd_length_reject" 1>&2 || {
    echo "not ok 1 - Reject dac odd length"
    exit 0
}
echo "ok 1 - Reject dac odd length"
exit 0
