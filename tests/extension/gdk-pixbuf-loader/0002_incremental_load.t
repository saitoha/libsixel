#!/bin/sh
# TAP wrapper that dispatches to the unified C test runner.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_GDK_PIXBUF2-}" = 1 || {
    printf "1..0 # SKIP gdk-pixbuf2 support is disabled in this build"
    exit 0
}

echo "1..1"
set -v

run_test_runner "gdk-pixbuf-loader/0002_incremental_load" >/dev/null || {
    echo "not ok 1 - gdk-pixbuf-loader/0002_incremental_load"
    exit 0
}

echo "ok 1 - gdk-pixbuf-loader/0002_incremental_load"
exit 0
