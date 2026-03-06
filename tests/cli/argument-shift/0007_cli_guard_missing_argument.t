#!/bin/sh
# TAP test for cli_guard_missing_argument handling of missing and leading dash.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

rc=0
cli_output_file="${ARTIFACT_LOCAL_DIR}/cli_guard_missing_argument.out"

run_test_runner "cli/0031_cli_guard_missing_argument" >"${cli_output_file}" 2>&1 || rc=$?
cat "${cli_output_file}" >&2 2>/dev/null || :

test "${rc}" -eq 0 || {
    echo "not ok" 1 - "cli_guard_missing_argument"
    exit 0
}

echo "ok" 1 - "cli_guard_missing_argument"
exit 0
