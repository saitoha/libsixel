#!/bin/sh
# TAP test for cli_guard_missing_argument handling of missing and leading dash.

set -eux


echo "1..1"
set -v

rc=0
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "cli/0031_cli_guard_missing_argument" || rc=$?
set -e

test "${rc}" -eq 0 || {
    echo "not ok" 1 - "cli_guard_missing_argument"
    exit 0
}

echo "ok" 1 - "cli_guard_missing_argument"
exit 0
