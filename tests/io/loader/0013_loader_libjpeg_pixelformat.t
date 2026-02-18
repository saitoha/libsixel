#!/bin/sh
# TAP wrapper that dispatches to the unified C test runner.

set -eux

test "${HAVE_JPEG-}" = 1 || {
    skip_all "libjpeg loader is unavailable"
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

run_test_runner "loader/0013_loader_libjpeg_pixelformat" || {
    echo "not ok 1 - loader/0013_loader_libjpeg_pixelformat"
    exit 0
}

echo "ok 1 - loader/0013_loader_libjpeg_pixelformat"
exit 0
