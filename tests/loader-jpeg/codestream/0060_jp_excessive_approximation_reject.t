#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject excessive approximation.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0110_jp_excessive_approximation_reject" 1>&2 || {
    echo "not ok 1 - Reject excessive approximation"
    exit 0
}
echo "ok 1 - Reject excessive approximation"
exit 0
