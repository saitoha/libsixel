#!/bin/sh
# TAP test verifying invalid kmeans autoratio values are rejected.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Qk:r=0 "${TOP_SRCDIR}/tests/data/inputs/small.ppm" -o/dev/null 2>&1) && {
    echo "not ok" 1 - "invalid kmeans autoratio unexpectedly succeeded"
    exit 0
}

test "${msg#*autoratio must be in range 1-1048576.*}" != "${msg}" || {
    echo "not ok" 1 - "missing kmeans autoratio range diagnostic"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok" 1 - "invalid kmeans autoratio is rejected"
exit 0
