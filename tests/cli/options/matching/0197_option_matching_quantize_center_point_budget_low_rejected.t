#!/bin/sh
# TAP test verifying -Q rejects non-zero center point_budget lower than 64.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qcenter:point_budget=63 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "invalid center point_budget unexpectedly succeeded"
    exit 0
}

test "${msg#*point_budget must be 0 or in range 64-16384.*}" != "${msg}" || {
    echo "not ok" 1 - "missing center point_budget range diagnostic"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok" 1 - "center point_budget lower bound is rejected"
exit 0
