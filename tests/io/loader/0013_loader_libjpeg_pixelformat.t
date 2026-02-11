#!/bin/sh
# TAP wrapper that dispatches to the unified C test runner.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

feature_defined_in_config "HAVE_LIBJPEG" || {
    skip_all "libjpeg loader is unavailable"
    exit 0
}

echo "1..1"
set -v

run_test_runner "loader/0013_loader_libjpeg_pixelformat.t" || {
    echo "not ok 1 - loader/0013_loader_libjpeg_pixelformat.t"
    exit 0
}

echo "ok 1 - loader/0013_loader_libjpeg_pixelformat.t"
exit 0
