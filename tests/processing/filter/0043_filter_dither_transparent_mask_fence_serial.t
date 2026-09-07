#!/bin/sh
# Run the focused serial transparent-mask dither fence test.
# Policy: docs/concepts/pixelformat.md

set -eux

test "${RUNTIME_ENV_BUILD_OS-}" != windows || {
    printf "1..0 # SKIP transparent-mask fence probe is unavailable on Windows\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0043_filter_dither_transparent_mask_fence_serial" || {
    echo "not ok 1 - serial transparent-mask dither fence"
    exit 0
}

echo "ok 1 - serial transparent-mask dither fence"
exit 0
