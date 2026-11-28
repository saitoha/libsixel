#!/bin/sh
set -eux
echo '1..1'
set -v

status=0
SIXEL_PALETTE_SAMPLE_TARGET=2 SIXEL_FRAME_YCBCR_CHROMA=linear \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical \
    nv21-lifecycle prepared || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' 'ok 1 # SKIP preparation overlap unavailable'
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - NV21 retains both planes through prepared cancellation'
    exit 0
}
printf '\n%s\n' 'ok 1 - NV21 retains both planes through prepared cancellation'
exit 0
