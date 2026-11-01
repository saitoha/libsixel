#!/bin/sh
# Verify paired palette content change with injected durations.
# Policy: docs/cli/threading-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0094_palette_auto_content_change || {
    echo "not ok 1 - palette auto content change"
    exit 0
}
echo "ok 1 - palette auto content change"
exit 0
