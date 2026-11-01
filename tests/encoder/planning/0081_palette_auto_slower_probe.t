#!/bin/sh
# Verify adaptive palette slower probe with injected durations.
# Policy: docs/cli/threading-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0081_palette_auto_slower_probe || {
    echo "not ok 1 - palette auto slower probe"
    exit 0
}
echo "ok 1 - palette auto slower probe"
exit 0
