#!/bin/sh
# Policy: docs/concepts/pixelformat-precision/README.md
# Verify exact subnormal or zero samples through a two-contributor mean.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_SIMD_LEVEL=scalar \
    frame/0018_float_resampling tiny_zero_y 4 || {
    printf '\n%s\n' "not ok 1 - float tiny_zero_y_scalar"
    exit 0
}
printf '\n%s\n' "ok 1 - float tiny_zero_y_scalar"
exit 0
