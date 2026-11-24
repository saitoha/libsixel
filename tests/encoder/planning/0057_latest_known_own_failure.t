#!/bin/sh
# Verify known own failure prevents output.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled"
    exit 0
}

echo "1..1"
set -v

result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0057_latest_known_own_failure || result=$?
test "$result" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP asynchronous palette scheduling is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    printf '\n%s\n' "not ok 1 - known own failure prevents output"
    exit 0
}
printf '\n%s\n' "ok 1 - known own failure prevents output"
exit 0
