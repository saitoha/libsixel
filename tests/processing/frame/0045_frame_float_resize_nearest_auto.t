#!/bin/sh
# Policy: docs/concepts/pixelformat-precision/README.md
# Verify float frame resize nearest auto.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_SIMD_LEVEL=auto \
    frame/0018_float_resampling nearest 0 || {
    echo "not ok 1 - float resize nearest_auto"
    exit 0
}
echo "ok 1 - float resize nearest_auto"
exit 0
