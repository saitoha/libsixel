#!/bin/sh
# Policy: docs/concepts/pixelformat-precision/README.md
# Verify float frame resize constant bilinear auto.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_SIMD_LEVEL=auto \
    frame/0018_float_resampling constant 4 || {
    printf '\n%s\n' "not ok 1 - float resize constant_bilinear_auto"
    exit 0
}
printf '\n%s\n' "ok 1 - float resize constant_bilinear_auto"
exit 0
