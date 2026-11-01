#!/bin/sh
# Verify palette launch sync sample.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0044_palette_launch_sync_sample || {
    echo "not ok 1 - palette launch sync sample"
    exit 0
}
echo "ok 1 - palette launch sync sample"
exit 0
