#!/bin/sh
# Policy: docs/concepts/pixelformat-precision/README.md
# Verify float frame resize constant hamming scalar.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_SIMD_LEVEL=scalar \
    frame/0018_float_resampling constant 3 || {
    printf '\n%s\n' "not ok 1 - float resize constant_hamming_scalar"
    exit 0
}
printf '\n%s\n' "ok 1 - float resize constant_hamming_scalar"
exit 0
