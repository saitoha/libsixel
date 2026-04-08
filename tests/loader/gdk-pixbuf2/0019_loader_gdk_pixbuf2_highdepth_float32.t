#!/bin/sh
# TAP wrapper for gdk-pixbuf2 pixelformat case: highdepth float32.

set -eux

test "${HAVE_GDK_PIXBUF2-}" = 1 || {
    printf "1..0 # SKIP gdk-pixbuf2 loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0010_loader_gdk_pixbuf_pixelformat" "highdepth_float32" || {
    echo "not ok 1 - gdk-pixbuf2 highdepth_float32"
    exit 0
}

echo "ok 1 - gdk-pixbuf2 highdepth_float32"
exit 0
