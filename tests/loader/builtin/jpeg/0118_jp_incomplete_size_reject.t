#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject incomplete size.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0118_jp_incomplete_size_reject" 1>&2 || {
    echo "not ok 1 - Reject incomplete size"
    exit 0
}
echo "ok 1 - Reject incomplete size"
exit 0
