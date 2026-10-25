#!/bin/sh
# Verify palette store own error.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0036_palette_store_own_error || {
    echo "not ok 1 - palette store own error"
    exit 0
}
echo "ok 1 - palette store own error"
exit 0
