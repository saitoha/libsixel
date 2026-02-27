#!/bin/sh
# TAP test verifying abort trace output from the runner test.

set -eux

test "${SIXEL_TSAN_BUILD:-no}" = "yes" && {
    printf "1..0 # SKIP TSan builds can suppress abort trace output\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

abort_output=$(run_test_runner --env SIXEL_ABORT_TRACE=1 \
    "aborttrace/0001_img2sixel_aborttrace" 2>&1) || rc=$?
printf '%s' "${abort_output}" >&2

test "${rc:-0}" -eq 77 && {
    pass 1 "abort trace # SKIP abort trace disabled"
    exit 0
}

printf '%s' "${abort_output}" | grep "libsixel: abort() detected" >/dev/null || {
    fail 1 "abort trace missing"
    exit 0
}
printf '%s' "${abort_output}" | grep "libsixel: abort trace complete" >/dev/null || {
    fail 1 "abort trace missing"
    exit 0
}

pass 1 "abort trace emitted"

exit 0
