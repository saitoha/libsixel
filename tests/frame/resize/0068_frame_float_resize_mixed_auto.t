#!/bin/sh
# Policy: docs/concepts/pixelformat-precision/README.md
# Verify finite nonconstant large values against independent normalized weights.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_SIMD_LEVEL=auto \
    frame/0018_float_resampling mixed 4 || {
    printf '\n%s\n' "not ok 1 - float resize mixed auto"
    exit 0
}
printf '\n%s\n' "ok 1 - float resize mixed auto"
exit 0
