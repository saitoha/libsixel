#!/bin/sh
set -eux

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_LOADER_COREGRAPHICS_CACHE_MAX_BYTES=1048576" \
    --env "SIXEL_TEST_COREGRAPHICS_STATIC_START_FRAME_CACHE_INERT=1" \
    "loader/0008_loader_coregraphics_pixelformat" || {
    echo "not ok 1 - loader/0008_loader_coregraphics_pixelformat (static start-frame cache inert)"
    exit 0
}

echo "ok 1 - loader/0008_loader_coregraphics_pixelformat (static start-frame cache inert)"
exit 0
