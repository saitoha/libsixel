#!/bin/sh
# Run the GPU blue-noise 6delta regression when Metal is available.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

# Keep capability initialization and real dispatch in one timed process.
status=0

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette \
    "filter/0025_filter_dither_6delta_bluenoise_gpu" || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - 0025_filter_dither_6delta_bluenoise_gpu"
    exit 0
}

echo "ok 1 - 0025_filter_dither_6delta_bluenoise_gpu"
exit 0
