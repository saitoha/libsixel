#!/bin/sh
# TAP wrapper that dispatches to the unified C test runner.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

run_test_runner "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat"
