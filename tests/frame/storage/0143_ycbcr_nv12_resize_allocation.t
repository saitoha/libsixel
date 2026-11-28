#!/bin/sh
set -eux
echo '1..1'
set -v

SIXEL_FRAME_YCBCR_CHROMA=linear \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical \
    ycbcr420 nv12 resize-failure || {
    printf '\n%s\n' 'not ok 1 - NV12 resize allocation failures preserve borrowed frame'
    exit 0
}
printf '\n%s\n' 'ok 1 - NV12 resize allocation failures preserve borrowed frame'
exit 0
