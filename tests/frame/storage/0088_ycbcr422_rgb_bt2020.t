#!/bin/sh
set -eux
echo '1..1'
set -v

SIXEL_FRAME_YCBCR422_CHROMA=linear \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical \
    ycbcr422-rgb bt2020 || {
    printf '\n%s\n' 'not ok 1 - BT.2020 NCL decodes both packed layouts and chroma positions'
    exit 0
}
printf '\n%s\n' 'ok 1 - BT.2020 NCL decodes both packed layouts and chroma positions'
exit 0
