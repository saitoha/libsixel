#!/bin/sh
# TAP runner for sixel_parse_header coverage cases.

set -eux

echo "1..1"
set -v

probe_output=$(${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "probe/0001_probe_parse" 2>&1) || rc=$?
printf '%s' "${probe_output}" >&2


test "${rc:-0}" -eq 0 || {
    echo "not ok" 1 - "probe_parse"
    exit 0
}

echo "ok" 1 - "probe_parse"

exit 0
