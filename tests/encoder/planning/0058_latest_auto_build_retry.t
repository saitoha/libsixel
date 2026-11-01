#!/bin/sh
# Verify auto own build retries from full frame.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0058_latest_auto_build_retry || {
    echo "not ok 1 - auto own build retries from full frame"
    exit 0
}
echo "ok 1 - auto own build retries from full frame"
exit 0
