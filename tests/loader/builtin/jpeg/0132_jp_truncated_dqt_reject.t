#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject truncated dqt.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0132_jp_truncated_dqt_reject" 1>&2 || {
    echo "not ok 1 - Reject truncated dqt"
    exit 0
}
echo "ok 1 - Reject truncated dqt"
exit 0
