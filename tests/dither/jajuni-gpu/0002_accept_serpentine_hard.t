#!/bin/sh
# Verify accept serpentine hard.
# Policy: docs/encoder/palette-application/dithering/jajuni-gpu.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "dither/jajuni-gpu/0002_accept_serpentine_hard" || status=$?
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - accept serpentine hard"
    exit 0
}
printf '\n%s\n' "ok 1 - accept serpentine hard"
exit 0
