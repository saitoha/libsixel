#!/bin/sh
set -eux
echo '1..1'
set -v

status=0
SIXEL_FRAME_YCBCR422_CHROMA=linear \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical \
    ycbcr422-adaptive-source || status=$?
test "$status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - Adaptive palette sampling converts only the grid'
    exit 0
}
printf '\n%s\n' 'ok 1 - Adaptive palette sampling converts only the grid'
exit 0
