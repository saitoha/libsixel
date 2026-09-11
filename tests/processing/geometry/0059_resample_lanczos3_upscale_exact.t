#!/bin/sh
# Compare lanczos3 non-integer 3:4 enlargement with the independent PPM oracle.
# Policy: docs/functionality/resampling/wide-kernels.md
set -eux

echo "1..1"
set -v

fixture_root="${TOP_SRCDIR}/tests/data/inputs/resampling-exact"

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_SIMD_LEVEL=scalar \
    --env SIXEL_THREADS=1 \
    geometry/resampling_exact \
    lanczos3 \
    "${fixture_root}/lanczos3-upscale-input.ppm" \
    "${fixture_root}/lanczos3-upscale-expected.ppm" || {
    echo "not ok" 1 - "lanczos3 upscale pixels changed"
    exit 0
}

echo "ok" 1 - "lanczos3 upscale pixels match the oracle"
exit 0
