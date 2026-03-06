#!/bin/sh
# TAP runner for sixel_parse_header coverage cases.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"
set -v

binary="${TEST_RUNNER_PATH}"
test -x "${binary}" || test -n "${SIXEL_RUNTIME-}" || {
    printf "1..0 # SKIP harness not built\n";
    exit 0
}

probe_output=$(run_test_runner "probe/0001_probe_parse" 2>&1) || rc=$?
printf '%s' "${probe_output}" >&2


test "${rc:-0}" -eq 0 || {
    echo "not ok" 1 - "probe_parse"
    exit 0
}

echo "ok" 1 - "probe_parse"

exit 0
