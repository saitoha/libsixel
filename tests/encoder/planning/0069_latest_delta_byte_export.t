#!/bin/sh
# Policy: docs/cli/threading-policy.md
# Verify latest delta byte export.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0069_latest_delta_byte_export || {
    echo "not ok 1 - latest delta byte export"
    exit 0
}
echo "ok 1 - latest delta byte export"
exit 0
