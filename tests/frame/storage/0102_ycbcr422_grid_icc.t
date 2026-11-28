#!/bin/sh
set -eux
echo '1..1'
set -v

status=0
SIXEL_FRAME_YCBCR_CHROMA=linear \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical \
    ycbcr422-grid-icc || status=$?
test "$status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - Sparse RGB matches the source coordinates and shares ICC'
    exit 0
}
printf '\n%s\n' 'ok 1 - Sparse RGB matches the source coordinates and shares ICC'
exit 0
