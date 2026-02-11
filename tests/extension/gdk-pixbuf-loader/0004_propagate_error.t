#!/bin/sh
# TAP wrapper that dispatches to the unified C test runner.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_feature_available "HAVE_GDK_PIXBUF2" "gdk_pixbuf_loader" \
    "gdk-pixbuf loader"

echo "1..1"
set -v

run_test_runner "gdk-pixbuf-loader/0004_propagate_error" >/dev/null || {
    echo "not ok 1 - gdk-pixbuf-loader/0004_propagate_error"
    exit 0
}

echo "ok 1 - gdk-pixbuf-loader/0004_propagate_error"
exit 0
