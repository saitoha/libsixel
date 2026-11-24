#!/bin/sh
# Verify byte snapshot private application.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0028_palette_snapshot_bytes || {
    printf '\n%s\n' "not ok 1 - byte snapshot private application"
    exit 0
}
printf '\n%s\n' "ok 1 - byte snapshot private application"
exit 0
