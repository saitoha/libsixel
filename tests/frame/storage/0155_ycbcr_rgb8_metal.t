#!/bin/sh
set -eux
echo '1..1'
set -v

status=0
SIXEL_FRAME_YCBCR_CHROMA=linear \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical \
    ycbcr420 nv12 rgb8-metal || status=$?
test "$status" != 77 || {
    printf '\n%s\n' 'ok 1 - YCbCr RGB8 metal # SKIP GPU unavailable'
    exit 0
}
test "$status" = 0 || {
    printf '\n%s\n' 'not ok 1 - YCbCr RGB8 metal matches CPU'
    exit 0
}
printf '\n%s\n' 'ok 1 - YCbCr RGB8 metal matches CPU'
exit 0
