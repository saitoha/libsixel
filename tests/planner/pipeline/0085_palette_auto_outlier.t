#!/bin/sh
# Verify adaptive palette outlier with injected durations.
# Policy: docs/cli/threading-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0085_palette_auto_outlier || {
    echo "not ok 1 - palette auto outlier"
    exit 0
}
echo "ok 1 - palette auto outlier"
exit 0
