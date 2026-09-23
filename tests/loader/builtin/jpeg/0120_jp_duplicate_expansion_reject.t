#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject duplicate expansion.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0120_jp_duplicate_expansion_reject" 1>&2 || {
    echo "not ok 1 - Reject duplicate expansion"
    exit 0
}
echo "ok 1 - Reject duplicate expansion"
exit 0
