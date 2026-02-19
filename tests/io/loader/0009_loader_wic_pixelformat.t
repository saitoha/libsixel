#!/bin/sh
# TAP wrapper that dispatches to the unified C test runner.

set -eux

test "${HAVE_WIC-}" = 1 || {
    printf "1..0 # SKIP wic loader is unavailable\n"
    exit 0
}
test "${RUNTIME_ENV_IS_WINE-0}" -eq 1 && {
    printf "1..0 # SKIP WIC is unavailable under wine\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

loader_output=$(run_test_runner "loader/0009_loader_wic_pixelformat" 2>&1) || rc=$?
printf '%s' "${loader_output}" >&2

test "${rc-0}" -eq 0 || {
    fail 1 "loader/0009_loader_wic_pixelformat"
    exit 0
}

pass 1 "loader/0009_loader_wic_pixelformat"
exit 0
