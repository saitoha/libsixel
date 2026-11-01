#!/bin/sh
# Policy: docs/threading/encoder.md
# Verify playback cancellation and failure complete frames in order.
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "planner/pipeline/0022_playback_cancel_failure"
result=$?
set -e
test "$result" -ne 77 || {
    echo "ok 1 - playback cancellation and failure complete frames in order # SKIP thread support disabled"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - playback cancellation and failure complete frames in order"
    exit 0
}
echo "ok 1 - playback cancellation and failure complete frames in order"
exit 0
