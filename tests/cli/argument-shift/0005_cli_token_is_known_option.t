#!/bin/sh
# TAP test covering cli_token_is_known_option with short and long tokens.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test -x "${TEST_RUNNER_PATH}" || [ -n "${SIXEL_RUNTIME-}" ] ||     skip_all "harness not built"

rc=0
cli_output_file="${ARTIFACT_LOCAL_DIR}/cli_token_is_known_option.out"

: >"${cli_output_file}"
run_test_runner "cli/0029_cli_token_is_known_option" >"${cli_output_file}" 2>&1 || rc=$?
cat "${cli_output_file}" >&2 2>/dev/null || :

echo "1..1"
set -v

test "${rc}" -eq 0 || {
    fail 1 "cli_token_is_known_option"
    exit 0
}

pass 1 "cli_token_is_known_option"
exit 0
