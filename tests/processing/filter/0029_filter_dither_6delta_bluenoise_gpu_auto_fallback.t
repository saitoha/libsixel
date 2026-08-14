#!/bin/sh
# AUTO must use the origin-aware CPU path for an offset retained plane.

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

SIXEL_GPU_PALETTE_THRESHOLD=1 \
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0029_filter_dither_6delta_bluenoise_gpu_auto_fallback" || {
    echo "not ok 1 - 0029 AUTO offset-plane CPU fallback"
    exit 0
}

echo "ok 1 - 0029 AUTO offset-plane CPU fallback"
exit 0
