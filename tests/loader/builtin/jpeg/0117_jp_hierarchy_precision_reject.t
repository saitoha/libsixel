#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject hierarchy precision.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0117_jp_hierarchy_precision_reject" 1>&2 || {
    echo "not ok 1 - Reject hierarchy precision"
    exit 0
}
echo "ok 1 - Reject hierarchy precision"
exit 0
