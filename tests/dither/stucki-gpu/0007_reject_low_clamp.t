#!/bin/sh
# Verify reject low clamp.
# Policy: docs/encoder/palette-application/dithering/stucki-gpu.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "dither/stucki-gpu/0007_reject_low_clamp" || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - reject low clamp"
    exit 0
}
echo "ok 1 - reject low clamp"
exit 0
