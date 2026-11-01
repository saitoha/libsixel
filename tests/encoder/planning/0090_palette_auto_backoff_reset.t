#!/bin/sh
# A reversed choice resets the reevaluation interval in both directions.
# Policy: docs/cli/threading-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0090_palette_auto_backoff_reset || {
    echo "not ok 1 - palette auto backoff reset"
    exit 0
}
echo "ok 1 - palette auto backoff reset"
exit 0
