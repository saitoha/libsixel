#!/bin/sh
set -eux
echo '1..1'
set -v

SIXEL_FRAME_YCBCR_CHROMA=linear \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical \
    ycbcr-shared clone || {
    printf '\n%s\n' 'not ok 1 - YCbCr clone owns compact rows after freeing the shared buffer'
    exit 0
}
printf '\n%s\n' 'ok 1 - YCbCr clone owns compact rows after freeing the shared buffer'
exit 0
