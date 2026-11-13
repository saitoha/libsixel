#!/bin/sh
# Verify reject negative deadzone.
# Policy: docs/encoder/palette-application/dithering/fs-gpu.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "dither/fs-gpu/0008_reject_negative_deadzone" || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - reject negative deadzone"
    exit 0
}
echo "ok 1 - reject negative deadzone"
exit 0
