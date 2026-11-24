#!/bin/sh
# Verify paired palette content change with injected durations.
# Policy: docs/cli/threading-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0094_palette_auto_content_change || {
    printf '\n%s\n' "not ok 1 - palette auto content change"
    exit 0
}
printf '\n%s\n' "ok 1 - palette auto content change"
exit 0
