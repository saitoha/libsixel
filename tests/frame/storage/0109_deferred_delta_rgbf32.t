#!/bin/sh
set -eux
echo '1..1'
set -v

status=0
SIXEL_FRAME_YCBCR_CHROMA=linear \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical \
    ycbcr422-deferred-delta rgbf32 || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' 'ok 1 # SKIP preparation overlap unavailable'
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - Deferred float32 delta emits both RGB float32 frames'
    exit 0
}
printf '\n%s\n' 'ok 1 - Deferred float32 delta emits both RGB float32 frames'
exit 0
