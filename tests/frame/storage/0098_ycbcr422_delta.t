#!/bin/sh
set -eux
echo '1..1'
set -v

SIXEL_FRAME_YCBCR_CHROMA=linear \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical \
    ycbcr422-encode delta || {
    printf '\n%s\n' 'not ok 1 - Packed 4:2:2 delta playback matches the RGB reference and paints one changed pixel'
    exit 0
}
printf '\n%s\n' 'ok 1 - Packed 4:2:2 delta playback matches the RGB reference and paints one changed pixel'
exit 0
