#!/bin/sh
# Verify adaptive palette faster probe with injected durations.
# Policy: docs/cli/threading-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0080_palette_auto_faster_probe || {
    echo "not ok 1 - palette auto faster probe"
    exit 0
}
echo "ok 1 - palette auto faster probe"
exit 0
