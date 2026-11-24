#!/bin/sh
# Verify adaptive palette zero wait with injected durations.
# Policy: docs/cli/threading-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0077_palette_auto_zero_wait || {
    printf '\n%s\n' "not ok 1 - palette auto zero wait"
    exit 0
}
printf '\n%s\n' "ok 1 - palette auto zero wait"
exit 0
