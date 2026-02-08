#!/bin/sh
# TAP runner for sixel_parse_header coverage cases.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

binary="${TEST_RUNNER_PATH}"
if [ ! -x "${binary}" ] && [ -z "${SIXEL_RUNTIME-}" ]; then
    echo "harness not built" >&2
    exit 99
fi

set +e
probe_output=$(run_test_runner "probe/0001_probe_parse" 2>&1)
rc=$?
set -e
printf '%s' "${probe_output}" >&2

echo "1..1"
set -v

if [ "${rc}" -eq 0 ]; then
    echo "ok 1 - probe_parse"
else
    echo "not ok 1 - probe_parse"
    exit 1
fi
