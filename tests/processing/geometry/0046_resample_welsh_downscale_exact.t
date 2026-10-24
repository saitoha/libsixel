#!/bin/sh
# Compare welsh strong 4:1 reduction with the independent PPM oracle.
# Policy: docs/encoder/preprocessing/resampling/compact-kernels.md
set -eux

echo "1..1"
set -v

fixture_root="${TOP_SRCDIR}/tests/data/inputs/resampling-exact"

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_SIMD_LEVEL=scalar \
    --env SIXEL_THREADING_GLOBAL_BUDGET=1 \
    geometry/resampling_exact \
    welsh \
    "${fixture_root}/welsh-downscale-input.ppm" \
    "${fixture_root}/welsh-downscale-expected.ppm" || {
    echo "not ok" 1 - "welsh downscale pixels changed"
    exit 0
}

echo "ok" 1 - "welsh downscale pixels match the oracle"
exit 0
