#!/bin/sh
# TAP wrapper that dispatches to the unified C test runner.

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng loader is unavailable\n"
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/0012_loader_libpng_pixelformat" || {
    echo "not ok 1 - loader/0012_loader_libpng_pixelformat"
    exit 0
}

echo "ok 1 - loader/0012_loader_libpng_pixelformat"
exit 0
