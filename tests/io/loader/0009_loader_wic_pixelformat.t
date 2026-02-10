#!/bin/sh
# TAP wrapper that dispatches to the unified C test runner.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

set +e
loader_output=$(run_test_runner "loader/${test_name}" 2>&1)
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
    echo "ok 1 - loader/${test_name}"
elif [ "${rc}" -eq 77 ]; then
    echo "ok 1 - loader/${test_name} # SKIP unavailable"
else
    echo "not ok 1 - loader/${test_name}"
fi
