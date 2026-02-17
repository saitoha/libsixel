#!/bin/sh
# TAP wrapper that dispatches to the unified C test runner.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

feature_defined_in_config "HAVE_WIC" || {
    skip_all "wic loader is unavailable"
}

set +e
loader_output=$(run_test_runner "loader/0009_loader_wic_pixelformat" 2>&1)
rc=$?
set -e
printf '%s' "${loader_output}" >&2

test "${RUNTIME_ENV_IS_WINE-0}" -eq 1 && {
    skip_all "WIC is unavailable under wine"
}

echo "1..1"
set -v

test "${rc}" -eq 0 || {
    fail 1 "loader/0009_loader_wic_pixelformat"
    exit 0
}

pass 1 "loader/0009_loader_wic_pixelformat"
exit 0
