#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject truncated eoi.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0093_jp_truncated_eoi_reject" 1>&2 || {
    echo "not ok 1 - Reject truncated eoi"
    exit 0
}
echo "ok 1 - Reject truncated eoi"
exit 0
