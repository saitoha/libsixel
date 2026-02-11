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

[ "${rc}" -eq 0 ] || [ "${rc}" -eq 77 ] || {
    echo "not ok 1 - loader/${test_name}"
    exit 0
}

[ "${rc}" -ne 77 ] || {
    echo "ok 1 - loader/${test_name} # SKIP unavailable"
    exit 0
}

echo "ok 1 - loader/${test_name}"
