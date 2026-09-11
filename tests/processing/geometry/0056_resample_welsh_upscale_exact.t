#!/bin/sh
# Compare welsh non-integer 3:4 enlargement with the independent PPM oracle.
# Policy: docs/functionality/resampling/compact-kernels.md
set -eux

echo "1..1"
set -v

fixture_root="${TOP_SRCDIR}/tests/data/inputs/resampling-exact"

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_SIMD_LEVEL=scalar \
    --env SIXEL_THREADS=1 \
    geometry/resampling_exact \
    welsh \
    "${fixture_root}/welsh-upscale-input.ppm" \
    "${fixture_root}/welsh-upscale-expected.ppm" || {
    echo "not ok" 1 - "welsh upscale pixels changed"
    exit 0
}

echo "ok" 1 - "welsh upscale pixels match the oracle"
exit 0
