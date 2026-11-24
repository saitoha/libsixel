#!/bin/sh
# TAP test for cli_option_requires_argument optstring parsing.

set -eux


echo "1..1"
set -v

rc=0
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "cli/0030_cli_option_requires_argument" || rc=$?
set -e

test "${rc}" -eq 0 || {
    printf '\n%s\n' "not ok 1 - cli_option_requires_argument"
    exit 0
}

printf '\n%s\n' "ok 1 - cli_option_requires_argument"
exit 0
