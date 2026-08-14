#!/bin/sh
# Run the GPU blue-noise 6delta regression when Metal is available.

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
    "filter/0025_filter_dither_6delta_bluenoise_gpu" || {
    echo "not ok 1 - 0025_filter_dither_6delta_bluenoise_gpu"
    exit 0
}

echo "ok 1 - 0025_filter_dither_6delta_bluenoise_gpu"
exit 0
