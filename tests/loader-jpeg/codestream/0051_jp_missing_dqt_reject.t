#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject missing dqt.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0101_jp_missing_dqt_reject" 1>&2 || {
    echo "not ok 1 - Reject missing dqt"
    exit 0
}
echo "ok 1 - Reject missing dqt"
exit 0
