#!/bin/sh
# TAP test verifying center budget_scale rejects values below range.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qcenter:budget_scale=0.20 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "out-of-range center budget_scale unexpectedly succeeded"
    exit 0
}

test "${msg#*budget_scale*}" != "${msg}" || {
    echo "not ok" 1 - "missing center budget_scale range diagnostic"
    exit 0
}

echo "ok" 1 - "center budget_scale rejects values below range"
exit 0
