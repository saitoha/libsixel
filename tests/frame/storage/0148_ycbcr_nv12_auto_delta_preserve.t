#!/bin/sh
set -eux
echo '1..1'
set -v

export SIXEL_PLANNER_RESIZE_PRECISION_MODE=preserve

status=0
SIXEL_PALETTE_SAMPLE_TARGET=2 SIXEL_FRAME_YCBCR_CHROMA=linear \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical \
    ycbcr420 nv12 auto-delta-preserve || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' 'ok 1 # SKIP preparation overlap unavailable'
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - NV12 delta retry reuses the plane resize'
    exit 0
}
printf '\n%s\n' 'ok 1 - NV12 delta retry reuses the plane resize'
exit 0
