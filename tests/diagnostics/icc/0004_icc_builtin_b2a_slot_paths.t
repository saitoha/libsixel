#!/bin/sh
# TAP runner for builtin ICC B2A0/B2A1/B2A2 parser/apply coverage.

set -eux

echo "1..1"
set -v

binary="${TEST_RUNNER_PATH}"
test -x "${binary}" || test -n "${SIXEL_RUNTIME-}" || {
    printf "1..0 # SKIP harness not built\n"
    exit 0
}

icc_output=$(${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "icc/0004_icc_builtin_b2a_slot_paths" 2>&1) || rc=$?
printf '%s' "${icc_output}" >&2

test "${rc:-0}" -eq 0 || {
    echo "not ok" 1 - "icc builtin B2A slot coverage"
    exit 0
}

echo "ok" 1 - "icc builtin B2A slot coverage"

exit 0
