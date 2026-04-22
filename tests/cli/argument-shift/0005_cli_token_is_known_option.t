#!/bin/sh
# TAP test covering cli_token_is_known_option with short and long tokens.

set -eux


echo "1..1"
set -v

rc=0

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cli/0029_cli_token_is_known_option" || rc=$?
set -e

test "${rc}" -eq 0 || {
    echo "not ok" 1 - "cli_token_is_known_option"
    exit 0
}

echo "ok" 1 - "cli_token_is_known_option"
exit 0
