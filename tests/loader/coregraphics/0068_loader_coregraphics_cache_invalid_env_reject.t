#!/bin/sh
set -eux

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_LOADER_COREGRAPHICS_CACHE_MAX_BYTES=invalid" \
    --env "SIXEL_TEST_COREGRAPHICS_CACHE_INVALID_ENV_REJECT=1" \
    "loader/0008_loader_coregraphics_pixelformat" || {
    echo "not ok 1 - loader/0008_loader_coregraphics_pixelformat (cache invalid env reject)"
    exit 0
}

echo "ok 1 - loader/0008_loader_coregraphics_pixelformat (cache invalid env reject)"
exit 0
