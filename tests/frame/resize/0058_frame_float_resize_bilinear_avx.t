#!/bin/sh
# Policy: docs/concepts/pixelformat-precision/README.md
# Verify bilinear values only when the compiled/runtime avx path is eligible.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_SIMD_LEVEL=avx \
    frame/0018_float_resampling bilinear 4 avx || status=$?
test "$status" -ne 77 || {
    echo "ok 1 - # SKIP avx float scaler is not available"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - float bilinear avx"
    exit 0
}
echo "ok 1 - float bilinear avx"
exit 0
