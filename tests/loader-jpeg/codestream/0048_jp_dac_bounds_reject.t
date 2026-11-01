#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject dac bounds.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0098_jp_dac_bounds_reject" 1>&2 || {
    echo "not ok 1 - Reject dac bounds"
    exit 0
}
echo "ok 1 - Reject dac bounds"
exit 0
