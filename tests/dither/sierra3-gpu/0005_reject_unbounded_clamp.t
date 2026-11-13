#!/bin/sh
# Verify reject unbounded clamp.
# Policy: docs/encoder/palette-application/dithering/sierra3-gpu.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "dither/sierra3-gpu/0005_reject_unbounded_clamp" || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - reject unbounded clamp"
    exit 0
}
echo "ok 1 - reject unbounded clamp"
exit 0
