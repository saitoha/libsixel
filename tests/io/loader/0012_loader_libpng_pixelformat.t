#!/bin/sh
# TAP wrapper that dispatches to the unified C test runner.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_LIBPNG-}" = 1 || {
    skip_all "libpng loader is unavailable"
}

echo "1..1"
set -v

run_test_runner "loader/0012_loader_libpng_pixelformat" || {
    echo "not ok 1 - loader/0012_loader_libpng_pixelformat"
    exit 0
}

echo "ok 1 - loader/0012_loader_libpng_pixelformat"
exit 0
