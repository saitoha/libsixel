#!/bin/sh
# TAP test for cli_option_requires_argument optstring parsing.

set -eux


echo "1..1"
set -v

rc=0
cli_output=''

cli_output=$(set +xv; ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "cli/0030_cli_option_requires_argument" 2>&1) || rc=$?
: "${cli_output}"

test "${rc}" -eq 0 || {
    printf '%s\n' "${cli_output}" >&2
    echo "not ok" 1 - "cli_option_requires_argument"
    exit 0
}

echo "ok" 1 - "cli_option_requires_argument"
exit 0
