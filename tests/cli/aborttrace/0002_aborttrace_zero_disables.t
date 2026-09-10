#!/bin/sh
# TAP test verifying that SIXEL_ABORT_TRACE=0 disables abort tracing.

set -eux

# The img2sixel regression suite covers this policy under Wine. The combined
# test runner can fail to terminate after this focused policy probe returns.
test "${SIXEL_RUNTIME-}" = "wine" && {
    printf "1..0 # SKIP wine can hang after abort trace policy probes\n"
    exit 0
}
test "${SIXEL_RUNTIME-}" = "wine64" && {
    printf "1..0 # SKIP wine can hang after abort trace policy probes\n"
    exit 0
}

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
