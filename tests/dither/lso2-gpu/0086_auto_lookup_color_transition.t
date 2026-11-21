#!/bin/sh
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    dither/lso2-gpu/0086_auto_lookup_color_transition || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP independent GPU playback unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - automatic lookup survives the exact-palette frame'
    exit 0
}
echo 'ok 1 - automatic lookup survives the exact-palette frame'
exit 0
