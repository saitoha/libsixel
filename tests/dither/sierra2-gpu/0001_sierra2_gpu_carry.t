#!/bin/sh
# Policy: docs/encoder/palette-application/dithering/sierra2-gpu.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "sierra2_gpu/carry" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP Metal device unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - sierra2 gpu carry'
    exit 0
}
echo 'ok 1 - sierra2 gpu carry'
exit 0
