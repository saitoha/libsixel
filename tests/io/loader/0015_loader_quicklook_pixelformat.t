#!/bin/sh
# TAP wrapper that dispatches to the unified C test runner.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

set +e
loader_output=$(run_test_runner "loader/${test_name}" 2>&1)
rc=$?
set -e
printf '%s' "${loader_output}" >&2

echo "1..1"
set -v

if [ "${rc}" -eq 0 ]; then
    echo "ok 1 - loader/${test_name}"
elif [ "${rc}" -eq 77 ]; then
    echo "ok 1 - loader/${test_name} # SKIP unavailable"
else
    echo "not ok 1 - loader/${test_name}"
fi
