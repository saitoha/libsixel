#!/bin/sh
# Verify float snapshot private application.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0029_palette_snapshot_float || {
    echo "not ok 1 - float snapshot private application"
    exit 0
}
echo "ok 1 - float snapshot private application"
exit 0
