#!/bin/sh
# Verify ladder capacity retry.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0134_ladder_capacity_retry" || {
    printf '\n%s\n' "not ok 1 - ladder capacity retry"
    exit 0
}
printf '\n%s\n' "ok 1 - ladder capacity retry"
exit 0
