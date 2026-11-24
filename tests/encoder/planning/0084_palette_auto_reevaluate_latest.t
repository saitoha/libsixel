#!/bin/sh
# Verify adaptive palette reevaluate latest with injected durations.
# Policy: docs/cli/threading-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0084_palette_auto_reevaluate_latest || {
    printf '\n%s\n' "not ok 1 - palette auto reevaluate latest"
    exit 0
}
printf '\n%s\n' "ok 1 - palette auto reevaluate latest"
exit 0
