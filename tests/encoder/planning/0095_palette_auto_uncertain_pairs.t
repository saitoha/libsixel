#!/bin/sh
# Verify paired palette uncertain pairs with injected durations.
# Policy: docs/cli/threading-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0095_palette_auto_uncertain_pairs || {
    printf '\n%s\n' "not ok 1 - palette auto uncertain pairs"
    exit 0
}
printf '\n%s\n' "ok 1 - palette auto uncertain pairs"
exit 0
