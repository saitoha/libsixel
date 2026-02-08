#!/bin/sh
# TAP test for cli_option_requires_argument optstring parsing.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

binary="${TEST_RUNNER_PATH}"
if [ ! -x "${binary}" ] && [ -z "${SIXEL_RUNTIME-}" ]; then
    echo "harness not built" >&2
    exit 99
fi

set +e
cli_output=$(run_test_runner "cli/0030_cli_option_requires_argument" 2>&1)
rc=$?
set -e
printf '%s' "${cli_output}" >&2

echo "1..1"
set -v

if [ "${rc}" -eq 0 ]; then
    echo "ok 1 - cli_option_requires_argument"
else
    echo "not ok 1 - cli_option_requires_argument"
    exit 1
fi
