#!/bin/sh
# Policy: docs/concepts/pixelformat-precision.md
# Verify bounded float coverage clamps each axis, independently of color.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_SIMD_LEVEL=auto \
    frame/0018_float_resampling bounded 6 || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - bounded float mask coverage auto"
    exit 0
}
echo "ok 1 - bounded float mask coverage auto"
exit 0
