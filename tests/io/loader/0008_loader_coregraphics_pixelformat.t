#!/bin/sh
# TAP wrapper that dispatches to the unified C test runner.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_COREGRAPHICS-}" = 1 || {
    skip_all "coregraphics loader is unavailable"
}

echo "1..1"
set -v

run_test_runner "loader/0008_loader_coregraphics_pixelformat" || {
    echo "not ok 1 - loader/0008_loader_coregraphics_pixelformat"
    exit 0
}

echo "ok 1 - loader/0008_loader_coregraphics_pixelformat"
exit 0
