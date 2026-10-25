#!/bin/sh
# Policy: docs/cli/threading-policy.md
# Verify palette snapshot allocation and cleanup.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0072_palette_snapshot_allocation || {
    echo "not ok 1 - palette snapshot allocation"
    exit 0
}
echo "ok 1 - palette snapshot allocation"
exit 0
