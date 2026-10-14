#!/bin/sh
# Verify step history invalidation.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0088_step_history_invalidation" || {
    echo "not ok 1 - step history invalidation"
    exit 0
}
echo "ok 1 - step history invalidation"
exit 0
