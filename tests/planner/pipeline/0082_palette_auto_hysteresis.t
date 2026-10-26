#!/bin/sh
# Verify adaptive palette hysteresis with injected durations.
# Policy: docs/cli/threading-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0082_palette_auto_hysteresis || {
    echo "not ok 1 - palette auto hysteresis"
    exit 0
}
echo "ok 1 - palette auto hysteresis"
exit 0
