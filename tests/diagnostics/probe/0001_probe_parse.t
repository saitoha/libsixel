#!/bin/sh
# TAP runner for sixel_parse_header coverage cases.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

binary="${TEST_RUNNER_PATH}"
test -x "${binary}" || test -n "${SIXEL_RUNTIME-}" || skip_all "harness not built"

probe_output=$(run_test_runner "probe/0001_probe_parse" 2>&1) || rc=$?
printf '%s' "${probe_output}" >&2

echo "1..1"
set -v

test "${rc:-0}" -eq 0 || {
    fail 1 "probe_parse"
    exit 0
}

pass 1 "probe_parse"

exit 0
