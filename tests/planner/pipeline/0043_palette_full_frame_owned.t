#!/bin/sh
# Verify palette full frame owned.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0043_palette_full_frame_owned || {
    echo "not ok 1 - palette full frame owned"
    exit 0
}
echo "ok 1 - palette full frame owned"
exit 0
