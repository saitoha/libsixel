#!/bin/sh
set -eux
echo '1..1'
set -v

SIXEL_FRAME_YCBCR_CHROMA=linear \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical \
    ycbcr-frame-view lifetime || {
    printf '\n%s\n' 'not ok 1 - YCbCr frame retains metadata and borrows pixels through conversion'
    exit 0
}
printf '\n%s\n' 'ok 1 - YCbCr frame retains metadata and borrows pixels through conversion'
exit 0
