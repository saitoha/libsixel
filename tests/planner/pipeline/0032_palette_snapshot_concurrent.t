#!/bin/sh
# Verify concurrent snapshot lifetime and private application.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0032_palette_snapshot_concurrent || {
    echo "not ok 1 - concurrent snapshot lifetime and private application"
    exit 0
}
echo "ok 1 - concurrent snapshot lifetime and private application"
exit 0
