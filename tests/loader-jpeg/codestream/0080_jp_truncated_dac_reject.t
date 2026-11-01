#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject truncated dac.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0130_jp_truncated_dac_reject" 1>&2 || {
    echo "not ok 1 - Reject truncated dac"
    exit 0
}
echo "ok 1 - Reject truncated dac"
exit 0
