#!/bin/sh
# TAP test covering cli_token_is_known_option with short and long tokens.

set -eux


echo "1..1"
set -v

rc=0
cli_output=''

cli_output=$(set +xv; ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "cli/0029_cli_token_is_known_option" 2>&1) || rc=$?
: "${cli_output}"

test "${rc}" -eq 0 || {
    printf '%s\n' "${cli_output}" >&2
    echo "not ok" 1 - "cli_token_is_known_option"
    exit 0
}

echo "ok" 1 - "cli_token_is_known_option"
exit 0
