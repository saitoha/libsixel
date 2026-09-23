#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject restart sequence.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0112_jp_restart_sequence_reject" 1>&2 || {
    echo "not ok 1 - Reject restart sequence"
    exit 0
}
echo "ok 1 - Reject restart sequence"
exit 0
