#!/bin/sh
# TAP wrapper that dispatches to the unified C test runner.

set -eux

test "${HAVE_LIBRSVG-}" = 1 || {
    printf "1..0 # SKIP librsvg loader is unavailable in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

run_test_runner "loader/0023_loader_librsvg_bgcolor_option" || rc="$?"

test "${rc-}" = 77 && {
    echo "ok 1 - loader/0023_loader_librsvg_bgcolor_option # SKIP unavailable"
    exit 0
}

test -n "${rc-}" && {
    echo "not ok 1 - loader/0023_loader_librsvg_bgcolor_option"
    exit 0
}

echo "ok 1 - loader/0023_loader_librsvg_bgcolor_option"
exit 0
