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

if printf '%s' "${loader_output}" \
    | grep "{cacaf262-9370-4615-a13b-9f5539da4c0a} not registered" \
        >/dev/null; then
    skip_all "WIC is not available"
fi

echo "1..1"
set -v

if [ "${rc}" -eq 0 ]; then
    echo "ok 1 - loader/0009_loader_wic_pixelformat"
else
    echo "not ok 1 - loader/0009_loader_wic_pixelformat"
fi
