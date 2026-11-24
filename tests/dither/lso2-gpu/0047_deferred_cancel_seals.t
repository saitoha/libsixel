#!/bin/sh
# Policy: docs/encoder/palette-application/dithering/lso2-gpu.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" lso2_gpu/deferred_cancel_seals || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' 'ok 1 # SKIP required backend unavailable'
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - deferred cancel seals'
    exit 0
}
printf '\n%s\n' 'ok 1 - deferred cancel seals'
exit 0
