#!/bin/sh
# Policy: docs/concepts/pixelformat-precision/README.md
# Verify float frame resize overflow auto.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_SIMD_LEVEL=auto \
    frame/0018_float_resampling overflow 6 || {
    echo "not ok 1 - float resize overflow_auto"
    exit 0
}
echo "ok 1 - float resize overflow_auto"
exit 0
