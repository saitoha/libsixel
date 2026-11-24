#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject zero sampling.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0103_jp_zero_sampling_reject" 1>&2 || {
    printf '\n%s\n' "not ok 1 - Reject zero sampling"
    exit 0
}
printf '\n%s\n' "ok 1 - Reject zero sampling"
exit 0
