#!/bin/sh
# Verify palette scheduler serial.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0045_palette_scheduler_serial || {
    echo "not ok 1 - palette scheduler serial"
    exit 0
}
echo "ok 1 - palette scheduler serial"
exit 0
