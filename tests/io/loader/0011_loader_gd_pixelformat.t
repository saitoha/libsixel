#!/bin/sh
# TAP wrapper that dispatches to the unified C test runner.

set -eux

test "${HAVE_DECL_GDIMAGECREATEFROMPNGPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMPNGPTR is unavailable\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

run_test_runner "loader/0011_loader_gd_pixelformat" || {
    echo "not ok 1 - loader/0011_loader_gd_pixelformat"
    exit 0
}

echo "ok 1 - loader/0011_loader_gd_pixelformat"
exit 0
