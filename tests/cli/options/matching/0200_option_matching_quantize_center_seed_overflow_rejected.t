#!/bin/sh
# TAP test verifying -Q rejects center seed values larger than uint32.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qcenter:seed=4294967296 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "invalid center seed unexpectedly succeeded"
    exit 0
}

test "${msg#*seed must be in range 0-4294967295.*}" != "${msg}" || {
    echo "not ok" 1 - "missing center seed range diagnostic"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok" 1 - "center seed upper bound is rejected"
exit 0
