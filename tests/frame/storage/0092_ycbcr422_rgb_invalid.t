#!/bin/sh
set -eux
echo '1..1'
set -v

SIXEL_FRAME_YCBCR_CHROMA=cubic \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical \
    ycbcr422-rgb invalid || {
    printf '\n%s\n' 'not ok 1 - Unknown chroma reconstruction policy rejects the stream definition'
    exit 0
}
printf '\n%s\n' 'ok 1 - Unknown chroma reconstruction policy rejects the stream definition'
exit 0
