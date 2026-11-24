#!/bin/sh
# Policy: docs/encoder/palette-application/dithering/lso2-gpu.md
set -eux
echo "1..1"
set -v
status=0
# A fresh Metal context must bind every span output on its first count pass.
MTL_DEBUG_LAYER=1 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" lso2_gpu/playback_single_slot || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP Metal device unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - lso2 gpu playback single slot"
    exit 0
}
printf '\n%s\n' "ok 1 - lso2 gpu playback single slot"
exit 0
