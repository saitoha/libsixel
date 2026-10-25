#!/bin/sh
# Verify palette scheduler concurrent.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0046_palette_scheduler_concurrent || {
    echo "not ok 1 - palette scheduler concurrent"
    exit 0
}
echo "ok 1 - palette scheduler concurrent"
exit 0
