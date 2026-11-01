#!/bin/sh
set -eux

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_COREGRAPHICS_WEBP_DELAY_REPLAY=1" \
    "loader/0008_loader_coregraphics_pixelformat" || {
    echo "not ok 1 - loader/0008_loader_coregraphics_pixelformat (webp delay replay consistency)"
    exit 0
}

echo "ok 1 - loader/0008_loader_coregraphics_pixelformat (webp delay replay consistency)"
exit 0
