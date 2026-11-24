#!/bin/sh
# Verify paired palette modest gain with injected durations.
# Policy: docs/cli/threading-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0093_palette_auto_modest_gain || {
    printf '\n%s\n' "not ok 1 - palette auto modest gain"
    exit 0
}
printf '\n%s\n' "ok 1 - palette auto modest gain"
exit 0
