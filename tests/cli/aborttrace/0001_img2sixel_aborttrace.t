#!/bin/sh
# TAP test verifying abort trace output from the runner test.

set -eux

test "${SIXEL_TSAN_BUILD:-no}" = "yes" && {
    printf "1..0 # SKIP TSan builds can suppress abort trace output\n";
    exit 0
}
test "${SIXEL_RUNTIME-}" = "wine" && {
    printf "1..0 # SKIP wine can hang on intentional abort handling\n";
    exit 0
}
test "${SIXEL_RUNTIME-}" = "wine64" && {
    printf "1..0 # SKIP wine can hang on intentional abort handling\n";
    exit 0
}


echo "1..1"
set -v

abort_output=$(${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --env SIXEL_ABORT_TRACE=1 \
    "aborttrace/0001_img2sixel_aborttrace" 2>&1) || rc=$?

test "${rc:-0}" -eq 77 && {
    echo "ok" 1 - "abort trace # SKIP abort trace disabled"
    exit 0
}

abort_detected=0
abort_complete=0
# Escape parentheses so ksh-derived /bin/sh treats them as literals.
abort_remainder=${abort_output#*libsixel: abort\(\) detected}
test "${abort_remainder}" != "${abort_output}" && abort_detected=1
abort_remainder=${abort_output#*libsixel: abort trace complete}
test "${abort_remainder}" != "${abort_output}" && abort_complete=1

test "${abort_detected}" = "1" || {
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${abort_output}" >&2
    echo "not ok" 1 - "abort trace missing"
    exit 0
}

test "${abort_complete}" = "1" || {
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${abort_output}" >&2
    echo "not ok" 1 - "abort trace missing"
    exit 0
}

echo "ok" 1 - "abort trace emitted"

exit 0
