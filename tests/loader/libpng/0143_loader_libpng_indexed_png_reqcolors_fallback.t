#!/bin/sh
# TAP wrapper for libpng indexed PNG reqcolors fallback C test.

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng loader is unavailable\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

run_test_runner "loader/0022_loader_libpng_indexed_png_reqcolors_fallback" || {
    echo "not ok 1 - loader/0022_loader_libpng_indexed_png_reqcolors_fallback"
    exit 0
}

echo "ok 1 - loader/0022_loader_libpng_indexed_png_reqcolors_fallback"
exit 0
