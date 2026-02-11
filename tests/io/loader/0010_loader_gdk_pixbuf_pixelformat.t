#!/bin/sh
# TAP wrapper that dispatches to the unified C test runner.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

feature_defined_in_config "HAVE_GDK_PIXBUF2" || {
    skip_all "gdk-pixbuf2 loader is unavailable"
    exit 0
}

echo "1..1"
set -v

run_test_runner "loader/0010_loader_gdk_pixbuf_pixelformat" || {
    echo "not ok 1 - loader/0010_loader_gdk_pixbuf_pixelformat"
    exit 0
}

echo "ok 1 - loader/0010_loader_gdk_pixbuf_pixelformat"
exit 0
