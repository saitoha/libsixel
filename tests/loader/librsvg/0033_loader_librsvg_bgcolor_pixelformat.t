#!/bin/sh

# Policy: docs/concepts/pixelformat.md

set -eux

test "${HAVE_LIBRSVG-}" = 1 || {
    printf "1..0 # SKIP librsvg loader is unavailable in this build\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0033_loader_librsvg_bgcolor_pixelformat" || {
    echo "not ok 1 - librsvg bgcolor pixelformat contract"
    exit 0
}

echo "ok 1 - librsvg bgcolor pixelformat contract"
exit 0
