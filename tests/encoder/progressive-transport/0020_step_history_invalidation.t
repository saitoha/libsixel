#!/bin/sh
# Verify step history invalidation.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0088_step_history_invalidation" || {
    printf '\n%s\n' "not ok 1 - step history invalidation"
    exit 0
}
printf '\n%s\n' "ok 1 - step history invalidation"
exit 0
