#!/bin/sh
# TAP wrapper that dispatches to the unified C test runner.

set -eux

test "${HAVE_LIBRSVG-}" = 1 || {
    printf "1..0 # SKIP librsvg loader is unavailable in this build\n"
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/0023_loader_librsvg_pixelformat" || rc="$?"

test "${rc-}" = 77 && {
    echo "ok 1 - loader/0023_loader_librsvg_pixelformat # SKIP unavailable"
    exit 0
}

test -n "${rc-}" && {
    echo "not ok 1 - loader/0023_loader_librsvg_pixelformat"
    exit 0
}

echo "ok 1 - loader/0023_loader_librsvg_pixelformat"
exit 0
