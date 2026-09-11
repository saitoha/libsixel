#!/bin/sh
# Compare gaussian strong 4:1 reduction with the independent PPM oracle.
# Policy: docs/functionality/resampling/compact-kernels.md
set -eux

echo "1..1"
set -v

fixture_root="${TOP_SRCDIR}/tests/data/inputs/resampling-exact"

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_SIMD_LEVEL=scalar \
    --env SIXEL_THREADS=1 \
    geometry/resampling_exact \
    gaussian \
    "${fixture_root}/gaussian-downscale-input.ppm" \
    "${fixture_root}/gaussian-downscale-expected.ppm" || {
    echo "not ok" 1 - "gaussian downscale pixels changed"
    exit 0
}

echo "ok" 1 - "gaussian downscale pixels match the oracle"
exit 0
