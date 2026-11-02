#!/bin/sh
# Policy: docs/cli/threading-policy.md
# Verify latest delta hls history.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled"
    exit 0
}

echo "1..1"
set -v

result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0063_latest_delta_hls_history || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP asynchronous palette scheduling is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - latest delta hls history"
    exit 0
}
echo "ok 1 - latest delta hls history"
exit 0
