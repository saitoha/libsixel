#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject trailing expansion.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0125_jp_trailing_expansion_reject" 1>&2 || {
    echo "not ok 1 - Reject trailing expansion"
    exit 0
}
echo "ok 1 - Reject trailing expansion"
exit 0
