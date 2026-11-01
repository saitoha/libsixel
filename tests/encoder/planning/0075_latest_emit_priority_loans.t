#!/bin/sh
# Policy: docs/cli/threading-policy.md
# Verify full emission admission and job-boundary loan reclamation.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0075_latest_emit_priority_loans || {
    echo "not ok 1 - latest emission has priority over borrowed slots"
    exit 0
}
echo "ok 1 - latest emission has priority over borrowed slots"
exit 0
