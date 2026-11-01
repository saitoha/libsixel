#!/bin/sh
# TAP wrapper for gdk-pixbuf2 pixelformat case: indexed alpha mask.

set -eux

test "${HAVE_GDK_PIXBUF2-}" = 1 || {
    printf "1..0 # SKIP gdk-pixbuf2 loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0010_loader_gdk_pixbuf_pixelformat" "indexed_alpha_mask" || {
    echo "not ok 1 - gdk-pixbuf2 indexed_alpha_mask"
    exit 0
}

echo "ok 1 - gdk-pixbuf2 indexed_alpha_mask"
exit 0
