#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject missing eoi.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0094_jp_missing_eoi_reject" 1>&2 || {
    printf '\n%s\n' "not ok 1 - Reject missing eoi"
    exit 0
}
printf '\n%s\n' "ok 1 - Reject missing eoi"
exit 0
