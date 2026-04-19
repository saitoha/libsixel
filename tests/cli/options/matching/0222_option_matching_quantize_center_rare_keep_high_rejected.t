#!/bin/sh
# TAP test verifying center rare_keep rejects values above range.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qcenter:rare_keep=2049 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "out-of-range center rare_keep unexpectedly succeeded"
    exit 0
}

test "${msg#*rare_keep*}" != "${msg}" || {
    echo "not ok" 1 - "missing center rare_keep range diagnostic"
    exit 0
}

echo "ok" 1 - "center rare_keep rejects values above range"
exit 0
