#!/bin/sh
set -eux
echo '1..1'
set -v

status=0
SIXEL_PALETTE_SAMPLE_TARGET=2 _SIXEL_TEST_PALETTE_JOB_FAILURE=worker SIXEL_FRAME_YCBCR422_CHROMA=linear \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical \
    ycbcr422-palette-color auto || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' 'ok 1 # SKIP required playback threading unavailable'
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - AUTO fallback converts only pixels left out of the grid'
    exit 0
}
printf '\n%s\n' 'ok 1 - AUTO fallback converts only pixels left out of the grid'
exit 0
