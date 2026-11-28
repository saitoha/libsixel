#!/bin/sh
set -eux
echo '1..1'
set -v

SIXEL_FRAME_YCBCR_CHROMA=nearest \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical \
    ycbcr422-rgb nearest || {
    printf '\n%s\n' 'not ok 1 - Nearest reconstructs chroma from each packed pair'
    exit 0
}
printf '\n%s\n' 'ok 1 - Nearest reconstructs chroma from each packed pair'
exit 0
