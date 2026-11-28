#!/bin/sh
set -eux
echo '1..1'
set -v

SIXEL_FRAME_YCBCR_CHROMA=linear \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical \
    ycbcr420 nv21 linear || {
    printf '\n%s\n' 'not ok 1 - NV21 chroma order and RGB values'
    exit 0
}
printf '\n%s\n' 'ok 1 - NV21 chroma order and RGB values'
exit 0
