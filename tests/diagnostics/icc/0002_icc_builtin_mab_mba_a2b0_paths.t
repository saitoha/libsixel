#!/bin/sh
# TAP runner for builtin ICC mAB/mBA A2B0 parser/apply coverage.

set -eux

echo "1..1"
set -v

icc_output=$(${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "icc/0002_icc_builtin_mab_mba_a2b0_paths" 2>&1) || rc=$?
printf '%s' "${icc_output}" >&2

test "${rc:-0}" -eq 0 || {
    echo "not ok" 1 - "icc builtin mAB/mBA A2B0 coverage"
    exit 0
}

echo "ok" 1 - "icc builtin mAB/mBA A2B0 coverage"

exit 0
