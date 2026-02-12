#!/bin/sh
# TAP test for cli_guard_missing_argument handling of missing and leading dash.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test -x "${TEST_RUNNER_PATH}" || [ -n "${SIXEL_RUNTIME-}" ] ||     skip_all "harness not built"

rc=0
cli_output_file="${ARTIFACT_LOCAL_DIR}/cli_guard_missing_argument.out"

: >"${cli_output_file}"
run_test_runner "cli/0031_cli_guard_missing_argument" >"${cli_output_file}" 2>&1 || rc=$?
cat "${cli_output_file}" >&2 2>/dev/null || :

echo "1..1"
set -v

test "${rc}" -eq 0 || {
    fail 1 "cli_guard_missing_argument"
    exit 0
}

pass 1 "cli_guard_missing_argument"
exit 0
