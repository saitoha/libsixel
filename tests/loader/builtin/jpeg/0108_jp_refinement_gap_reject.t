#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject refinement gap.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0108_jp_refinement_gap_reject" 1>&2 || {
    echo "not ok 1 - Reject refinement gap"
    exit 0
}
echo "ok 1 - Reject refinement gap"
exit 0
