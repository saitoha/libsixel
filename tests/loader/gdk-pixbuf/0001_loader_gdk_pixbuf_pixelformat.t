#!/bin/sh
# TAP wrapper that dispatches to the unified C test runner.

set -eux

test "${HAVE_GDK_PIXBUF2-}" = 1 || {
    printf "1..0 # SKIP gdk-pixbuf2 loader is unavailable\n"
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0010_loader_gdk_pixbuf_pixelformat" "rgba_no_bg_mask" || {
    echo "not ok 1 - gdk-pixbuf2 rgba_no_bg_mask"
    exit 0
}

echo "ok 1 - gdk-pixbuf2 rgba_no_bg_mask"
exit 0
