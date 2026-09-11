#!/bin/sh
# Compare resize_precision=preserve with a precomputed safe-tone PPM.
# Test-plan: docs/testing/resampling-coverage.md
set -eux

test -x "${IMG2SIXEL_PATH}" || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}
echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

fixture_root="${TOP_SRCDIR}/tests/data/inputs/resampling-exact"
actual_sixel="${ARTIFACT_LOCAL_DIR}/precision-preserve-actual.six"

set -- --threads=1 --precision=8bit --quality=full \
    '--loaders=builtin!' --cms-engine=none \
    --sampling-policy=full-frame --binning-policy=hard \
    -Xgamma -Wgamma -Ugamma --diffusion=none \
    --gpu-policy=off --lookup-policy=none \
    --palette-type=rgb --encode-policy=fast \
    -m "${fixture_root}/safe-gray.pal"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "$@" \
    -w3 -h1 -rbilinear \
    -jscalar:resize_precision=preserve \
    -o "${actual_sixel}" \
    "${fixture_root}/bilinear-precision-input.ppm" || {
    echo "not ok" 1 - "resize_precision=preserve failed"
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    geometry/resampling_exact decode "${actual_sixel}" \
    "${fixture_root}/bilinear-precision-preserve-expected.ppm" || {
    echo "not ok" 1 - "resize_precision=preserve pixels changed"
    exit 0
}

echo "ok" 1 - "resize_precision=preserve matches the safe-tone oracle"
exit 0
