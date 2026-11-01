#!/bin/sh
# Verify known own failure prevents output.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0057_latest_known_own_failure || {
    echo "not ok 1 - known own failure prevents output"
    exit 0
}
echo "ok 1 - known own failure prevents output"
exit 0
