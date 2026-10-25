#!/bin/sh
# Verify palette store stop join.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0037_palette_store_stop_join || {
    echo "not ok 1 - palette store stop join"
    exit 0
}
echo "ok 1 - palette store stop join"
exit 0
