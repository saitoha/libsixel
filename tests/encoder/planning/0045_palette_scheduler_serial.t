#!/bin/sh
# Verify palette scheduler serial.
# Policy: docs/threading/animation.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0045_palette_scheduler_serial || {
    printf '\n%s\n' "not ok 1 - palette scheduler serial"
    exit 0
}
printf '\n%s\n' "ok 1 - palette scheduler serial"
exit 0
