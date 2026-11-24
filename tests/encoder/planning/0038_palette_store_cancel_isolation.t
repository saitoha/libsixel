#!/bin/sh
# Verify palette store cancel isolation.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0038_palette_store_cancel_isolation || {
    printf '\n%s\n' "not ok 1 - palette store cancel isolation"
    exit 0
}
printf '\n%s\n' "ok 1 - palette store cancel isolation"
exit 0
