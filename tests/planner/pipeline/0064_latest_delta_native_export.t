#!/bin/sh
# Policy: docs/cli/threading-policy.md
# Verify latest delta native export.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0064_latest_delta_native_export || {
    echo "not ok 1 - latest delta native export"
    exit 0
}
echo "ok 1 - latest delta native export"
exit 0
