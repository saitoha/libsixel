#!/bin/sh
# Policy: docs/threading/encoder.md
# B1 reservations remain bounded without a worker
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0109_playback_serial_reservation || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP thread support disabled"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - B1 reservations remain bounded without a worker"
    exit 0
}
echo "ok 1 - B1 reservations remain bounded without a worker"
exit 0
