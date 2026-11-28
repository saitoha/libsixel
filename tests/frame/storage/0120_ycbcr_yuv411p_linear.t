#!/bin/sh
set -eux
echo '1..1'
set -v

SIXEL_FRAME_YCBCR_CHROMA=linear \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical \
    ycbcr411 linear || {
    printf '\n%s\n' 'not ok 1 - YUV411P linear chroma stays within each row'
    exit 0
}
printf '\n%s\n' 'ok 1 - YUV411P linear chroma stays within each row'
exit 0
