#!/bin/sh
# TAP test verifying that SIXEL_ABORT_TRACE=0 disables abort tracing.

set -eux

echo "1..1"
set -v

abort_output=$(${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_TRACE_TOPIC=aborttrace_contract \
    --env SIXEL_ABORT_TRACE=0 \
    "aborttrace/0001_img2sixel_aborttrace" install-only 2>&1) || rc=$?

test "${rc:-0}" -eq 77 && {
    echo "ok" 1 - "abort trace zero # SKIP abort trace disabled"
    exit 0
}

test "${rc:-0}" -eq 0 || {
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${abort_output}" >&2
    echo "not ok" 1 - "abort trace zero policy probe failed"
    exit 0
}

abort_remainder=${abort_output#*LSXABT1\|enabled=0\|installed=0*}
test "${abort_remainder}" != "${abort_output}" || {
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${abort_output}" >&2
    echo "not ok" 1 - "abort trace zero did not disable installation"
    exit 0
}

echo "ok" 1 - "abort trace zero disables tracing"
exit 0
