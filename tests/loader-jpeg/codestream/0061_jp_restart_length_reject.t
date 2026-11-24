#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject restart length.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0111_jp_restart_length_reject" 1>&2 || {
    printf '\n%s\n' "not ok 1 - Reject restart length"
    exit 0
}
printf '\n%s\n' "ok 1 - Reject restart length"
exit 0
