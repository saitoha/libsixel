#!/bin/sh
# Verify byte snapshot private application.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0028_palette_snapshot_bytes || {
    echo "not ok 1 - byte snapshot private application"
    exit 0
}
echo "ok 1 - byte snapshot private application"
exit 0
