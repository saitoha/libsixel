#!/bin/sh
# TAP wrapper that dispatches to the unified C test runner.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

feature_defined_in_config "HAVE_GD" || {
    skip_all "GD loader is unavailable"
}

echo "1..1"
set -v

run_test_runner "loader/0011_loader_gd_pixelformat" || {
    echo "not ok 1 - loader/0011_loader_gd_pixelformat"
    exit 0
}

echo "ok 1 - loader/0011_loader_gd_pixelformat"
exit 0
