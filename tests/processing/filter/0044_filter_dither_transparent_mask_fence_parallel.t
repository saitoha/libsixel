#!/bin/sh
# Run the focused parallel transparent-mask dither fence test.
# Policy: docs/concepts/pixelformat.md

set -eux

test "${RUNTIME_ENV_BUILD_OS-}" != windows || {
    printf "1..0 # SKIP transparent-mask fence probe is unavailable on Windows\n"
    exit 0
}

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP internal threading is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0044_filter_dither_transparent_mask_fence_parallel" || {
    echo "not ok 1 - parallel transparent-mask dither fence"
    exit 0
}

echo "ok 1 - parallel transparent-mask dither fence"
exit 0
