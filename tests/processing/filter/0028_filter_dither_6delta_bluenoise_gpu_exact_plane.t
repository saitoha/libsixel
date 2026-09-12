#!/bin/sh
# Exercise exact-plane 6delta wiring through high-level GPU blue-noise dither.
# Policy: docs/functionality/delta-encoding.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_GPU_POLICY=force \
    -d none -p 16 --lookup-policy=none -o /dev/null \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" || {
    echo "1..0 # SKIP forced GPU palette apply is unavailable"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0028_filter_dither_6delta_bluenoise_gpu_exact_plane" || {
    echo "not ok 1 - 0028 exact-plane GPU blue-noise 6delta"
    exit 0
}

echo "ok 1 - 0028 exact-plane GPU blue-noise 6delta"
exit 0
