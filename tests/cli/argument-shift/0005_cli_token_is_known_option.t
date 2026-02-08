#!/bin/sh
# TAP test covering cli_token_is_known_option with short/long tokens.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

binary="${TEST_RUNNER_PATH}"
if [ ! -x "${binary}" ] && [ -z "${SIXEL_RUNTIME-}" ]; then
    echo "harness not built" >&2
    exit 99
fi

set +e
cli_output=$(run_test_runner "cli/0029_cli_token_is_known_option" 2>&1)
rc=$?
set -e
printf '%s' "${cli_output}" >&2

echo "1..1"
set -v

if [ "${rc}" -eq 0 ]; then
    echo "ok 1 - cli_token_is_known_option"
else
    echo "not ok 1 - cli_token_is_known_option"
    exit 1
fi
