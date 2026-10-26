#!/bin/sh
# Verify adaptive palette reevaluate frame with injected durations.
# Policy: docs/cli/threading-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0083_palette_auto_reevaluate_frame || {
    echo "not ok 1 - palette auto reevaluate frame"
    exit 0
}
echo "ok 1 - palette auto reevaluate frame"
exit 0
