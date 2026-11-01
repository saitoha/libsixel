#!/bin/sh
# Compare lanczos4 strong 4:1 reduction with the independent PPM oracle.
# Policy: docs/encoder/preprocessing/resampling/wide-kernels.md
set -eux

echo "1..1"
set -v

fixture_root="${TOP_SRCDIR}/tests/data/inputs/resampling-exact"

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_SIMD_LEVEL=scalar \
    --env SIXEL_ENCODER_THREADING_POLICY=greedy --env SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    geometry/resampling_exact \
    lanczos4 \
    "${fixture_root}/lanczos4-downscale-input.ppm" \
    "${fixture_root}/lanczos4-downscale-expected.ppm" || {
    echo "not ok" 1 - "lanczos4 downscale pixels changed"
    exit 0
}

echo "ok" 1 - "lanczos4 downscale pixels match the oracle"
exit 0
