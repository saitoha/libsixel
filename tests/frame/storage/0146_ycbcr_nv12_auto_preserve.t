#!/bin/sh
set -eux
echo '1..1'
set -v

export SIXEL_PLANNER_RESIZE_PRECISION_MODE=preserve

SIXEL_FRAME_YCBCR_CHROMA=linear \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical \
    ycbcr420 nv12 auto-preserve || {
    printf '\n%s\n' 'not ok 1 - NV12 automatic resize preserves signal coordinates'
    exit 0
}
printf '\n%s\n' 'ok 1 - NV12 automatic resize preserves signal coordinates'
exit 0
