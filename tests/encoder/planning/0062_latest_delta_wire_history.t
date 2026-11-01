#!/bin/sh
# Policy: docs/cli/threading-policy.md
# Verify latest delta wire history.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0062_latest_delta_wire_history || {
    echo "not ok 1 - latest delta wire history"
    exit 0
}
echo "ok 1 - latest delta wire history"
exit 0
