#!/bin/sh
set -eux
echo '1..1'
set -v

SIXEL_FRAME_YCBCR_CHROMA=linear \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical \
    ycbcr420 i420 resize || {
    printf '\n%s\n' 'not ok 1 - I420 fused bilinear resize matches RGB reference'
    exit 0
}
printf '\n%s\n' 'ok 1 - I420 fused bilinear resize matches RGB reference'
exit 0
