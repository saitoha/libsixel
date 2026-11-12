#!/bin/sh
# Verify queued 6delta frames apply on the GPU under a forced policy, with
# apply and encode running as separate pipelined stages.
set -eux

echo "1..1"
set -v

# Keep capability initialization and real dispatch in one timed process.
status=0

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette-direct gpu-direct/0009_playback_force || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP direct GPU palette apply is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - forced GPU playback pipelines apply and encode for queued 6delta frames"
    exit 0
}
echo "ok 1 - forced GPU playback pipelines apply and encode for queued 6delta frames"
exit 0
