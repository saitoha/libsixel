#!/bin/sh
# TAP runner for builtin ICC device-to-device intent path coverage.

set -eux

echo "1..1"
set -v

binary="${TEST_RUNNER_PATH}"
test -x "${binary}" || test -n "${SIXEL_RUNTIME-}" || {
    printf "1..0 # SKIP harness not built\n"
    exit 0
}

icc_output=$(${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "icc/0005_icc_builtin_device_to_device_intent_paths" 2>&1) || rc=$?
printf '%s' "${icc_output}" >&2

test "${rc:-0}" -eq 0 || {
    echo "not ok" 1 - "icc builtin device-to-device intent coverage"
    exit 0
}

echo "ok" 1 - "icc builtin device-to-device intent coverage"

exit 0
