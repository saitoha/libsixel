#!/bin/sh
# Verify forced GPU playback applies and encodes 6delta frames, with
# separate pipeline stages when worker threads are enabled.
set -eux

echo "1..1"
set -v

# Keep capability initialization and real dispatch in one timed process.
status=0

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette-direct gpu-direct/0009_playback_force || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP direct GPU palette apply is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - forced GPU playback encodes 6delta frames with the expected scheduling"
    exit 0
}
printf '\n%s\n' "ok 1 - forced GPU playback encodes 6delta frames with the expected scheduling"
exit 0
