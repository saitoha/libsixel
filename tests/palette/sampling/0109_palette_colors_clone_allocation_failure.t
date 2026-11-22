#!/bin/sh
# A failed palette sample clone releases its frame and leaves the input intact.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/sampling/0109_palette_colors_clone_allocation_failure || {
    echo "not ok 1 - a failed palette sample clone leaves the input intact"
    exit 0
}
echo "ok 1 - a failed palette sample clone leaves the input intact"
exit 0
