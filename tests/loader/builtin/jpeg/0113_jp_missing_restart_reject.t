#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject missing restart.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0113_jp_missing_restart_reject" 1>&2 || {
    echo "not ok 1 - Reject missing restart"
    exit 0
}
echo "ok 1 - Reject missing restart"
exit 0
