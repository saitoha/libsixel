#!/bin/sh
# TAP test verifying -Q rejects center prune_mass lower than 0.900.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qcenter:prune_mass=0.899 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "invalid center prune_mass unexpectedly succeeded"
    exit 0
}

test "${msg#*prune_mass must be in range 0.900-1.000.*}" != "${msg}" || {
    echo "not ok" 1 - "missing center prune_mass range diagnostic"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok" 1 - "center prune_mass lower bound is rejected"
exit 0
