#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject duplicate sof.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0102_jp_duplicate_sof_reject" 1>&2 || {
    echo "not ok 1 - Reject duplicate sof"
    exit 0
}
echo "ok 1 - Reject duplicate sof"
exit 0
