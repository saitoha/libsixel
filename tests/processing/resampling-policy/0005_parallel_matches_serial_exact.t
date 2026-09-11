#!/bin/sh
# Compare multi-row parallel byte resize with the serial safe-tone oracle.
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
serial_sixel="${ARTIFACT_LOCAL_DIR}/thread-serial.six"
parallel_sixel="${ARTIFACT_LOCAL_DIR}/thread-parallel.six"

set -- --precision=8bit --quality=full \
    '--loaders=builtin!' --cms-engine=none \
    --sampling-policy=full-frame --binning-policy=hard \
    -Xgamma -Wgamma -Ugamma --diffusion=none \
    --gpu-policy=off --lookup-policy=none \
    --palette-type=rgb --encode-policy=fast \
    -m "${fixture_root}/safe-gray.pal"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "$@" \
    --threads=1 -w4 -h4 -rbilinear \
    -jscalar:resize_precision=preserve \
    -o "${serial_sixel}" \
    "${fixture_root}/bilinear-thread-input.ppm" || {
    echo "not ok" 1 - "serial resize failed"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "$@" \
    --threads=4 --env SIXEL_SCALE_PARALLEL_MIN_BYTES=0 \
    -w4 -h4 -rbilinear -jscalar:resize_precision=preserve \
    -o "${parallel_sixel}" \
    "${fixture_root}/bilinear-thread-input.ppm" || {
    echo "not ok" 1 - "parallel resize failed"
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    geometry/resampling_exact decode "${serial_sixel}" \
    "${fixture_root}/bilinear-thread-expected.ppm" || {
    echo "not ok" 1 - "serial resize diverged from the oracle"
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    geometry/resampling_exact decode "${parallel_sixel}" \
    "${fixture_root}/bilinear-thread-expected.ppm" || {
    echo "not ok" 1 - "parallel resize diverged from serial"
    exit 0
}

echo "ok" 1 - "parallel and serial resize match the safe-tone oracle"
exit 0
