#!/bin/sh
# Policy: docs/cli/threading-policy.md
# Verify latest thread failure same sample.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled"
    exit 0
}

echo "1..1"
set -v

result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0052_latest_thread_failure_same_sample || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP asynchronous palette scheduling is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - latest thread failure same sample"
    exit 0
}
echo "ok 1 - latest thread failure same sample"
exit 0
