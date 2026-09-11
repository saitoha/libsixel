#!/bin/sh
# Compare bicubic strong 4:1 reduction with the independent PPM oracle.
# Policy: docs/functionality/resampling/wide-kernels.md
set -eux

echo "1..1"
set -v

fixture_root="${TOP_SRCDIR}/tests/data/inputs/resampling-exact"

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_SIMD_LEVEL=scalar \
    --env SIXEL_THREADS=1 \
    geometry/resampling_exact \
    bicubic \
    "${fixture_root}/bicubic-downscale-input.ppm" \
    "${fixture_root}/bicubic-downscale-expected.ppm" || {
    echo "not ok" 1 - "bicubic downscale pixels changed"
    exit 0
}

echo "ok" 1 - "bicubic downscale pixels match the oracle"
exit 0
