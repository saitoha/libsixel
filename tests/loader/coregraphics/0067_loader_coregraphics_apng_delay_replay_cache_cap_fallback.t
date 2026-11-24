#!/bin/sh
set -eux

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_LOADER_COREGRAPHICS_CACHE_MAX_BYTES=1" \
    --env "SIXEL_TEST_COREGRAPHICS_APNG_DELAY_REPLAY=1" \
    "loader/0008_loader_coregraphics_pixelformat" || {
    printf '\n%s\n' "not ok 1 - loader/0008_loader_coregraphics_pixelformat (apng delay replay cache cap fallback)"
    exit 0
}

printf '\n%s\n' "ok 1 - loader/0008_loader_coregraphics_pixelformat (apng delay replay cache cap fallback)"
exit 0
