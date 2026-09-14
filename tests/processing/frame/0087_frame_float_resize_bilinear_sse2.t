#!/bin/sh
# Policy: docs/concepts/pixelformat-precision.md
# Verify bilinear values only when the compiled/runtime sse2 path is eligible.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_SIMD_LEVEL=sse2 \
    frame/0018_float_resampling bilinear 4 sse2 || status=$?
test "$status" -ne 77 || {
    echo "ok 1 - # SKIP sse2 float scaler is not available"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - float bilinear sse2"
    exit 0
}
echo "ok 1 - float bilinear sse2"
exit 0
