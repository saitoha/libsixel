#!/bin/sh
# TAP test verifying abort trace output from the runner test.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

[ "${SIXEL_TSAN_BUILD:-no}" = "yes" ] && skip_all "TSan builds can suppress abort trace output"

binary="${TEST_RUNNER_PATH}"
[ -x "${binary}" ] || [ -n "${SIXEL_RUNTIME-}" ] || skip_all "harness not built"

abort_output=$(run_test_runner --env SIXEL_ABORT_TRACE=1 -- \
    "aborttrace/0001_img2sixel_aborttrace" 2>&1) || rc=$?
printf '%s' "${abort_output}" >&2

echo "1..1"
set -v

[ "${rc:-0}" -eq 77 ] && {
    pass 1 "abort trace # SKIP abort trace disabled"
    exit 0
}

printf '%s' "${abort_output}" | grep -F "libsixel: abort() detected" >/dev/null || {
    fail 1 "abort trace missing"
    exit 0
}
printf '%s' "${abort_output}" | grep -F "libsixel: abort trace complete" >/dev/null || {
    fail 1 "abort trace missing"
    exit 0
}

pass 1 "abort trace emitted"

exit 0
