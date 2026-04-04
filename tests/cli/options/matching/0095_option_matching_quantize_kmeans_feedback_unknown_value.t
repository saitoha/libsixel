#!/bin/sh
# TAP test verifying unknown kmeans feedback values are rejected.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Qk:f=invalid "${TOP_SRCDIR}/tests/data/inputs/small.ppm" -o/dev/null 2>&1) && {
    echo "not ok" 1 - "unknown kmeans feedback value unexpectedly succeeded"
    exit 0
}

test "${msg#*unknown suboption value*}" != "${msg}" || {
    echo "not ok" 1 - "missing unknown suboption value diagnostic"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${msg#*\"feedback\"*}" != "${msg}" || {
    echo "not ok" 1 - "missing feedback key diagnostic"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${msg#*valid values*off, on*}" != "${msg}" || {
    echo "not ok" 1 - "missing kmeans feedback valid value list"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok" 1 - "unknown kmeans feedback value is rejected"
exit 0
