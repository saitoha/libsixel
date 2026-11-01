#!/bin/sh
# Policy: docs/threading/encoder.md
# Reservation release wakes a full-queue waiter
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0105_playback_wait_release || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP thread support disabled"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - Reservation release wakes a full-queue waiter"
    exit 0
}
echo "ok 1 - Reservation release wakes a full-queue waiter"
exit 0
