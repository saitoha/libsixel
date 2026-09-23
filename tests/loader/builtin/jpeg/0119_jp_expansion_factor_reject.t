#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject expansion factor.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0119_jp_expansion_factor_reject" 1>&2 || {
    echo "not ok 1 - Reject expansion factor"
    exit 0
}
echo "ok 1 - Reject expansion factor"
exit 0
