#!/bin/sh
# Verify adaptive palette probe length with injected durations.
# Policy: docs/cli/threading-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0079_palette_auto_probe_length || {
    echo "not ok 1 - palette auto probe length"
    exit 0
}
echo "ok 1 - palette auto probe length"
exit 0
