#!/bin/sh
# Confirmed choices double reevaluation intervals up to the cap.
# Policy: docs/cli/threading-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0089_palette_auto_backoff || {
    printf '\n%s\n' "not ok 1 - palette auto backoff"
    exit 0
}
printf '\n%s\n' "ok 1 - palette auto backoff"
exit 0
