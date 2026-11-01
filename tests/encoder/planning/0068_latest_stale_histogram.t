#!/bin/sh
# Policy: docs/cli/threading-policy.md
# Verify latest stale histogram.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0068_latest_stale_histogram || {
    echo "not ok 1 - latest stale histogram"
    exit 0
}
echo "ok 1 - latest stale histogram"
exit 0
