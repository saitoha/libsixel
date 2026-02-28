#!/bin/sh
# TAP wrapper that dispatches to the unified C test runner.

set -eux

test "${HAVE_GDK_PIXBUF2-}" = 1 || {
    printf "1..0 # SKIP gdk-pixbuf2 support is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

run_test_runner "gdk-pixbuf-loader/0003_corrupt_data" >/dev/null || {
    echo "not ok 1 - gdk-pixbuf-loader/0003_corrupt_data"
    exit 0
}

echo "ok 1 - gdk-pixbuf-loader/0003_corrupt_data"
exit 0
