#!/bin/sh
# TAP test verifying invalid stbn strength values are rejected.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

msg=$(
    set +xv
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -d stbn:strength=invalid \
        "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
        -o /dev/null 2>&1
) && {
    echo "not ok" 1 - "invalid stbn strength unexpectedly succeeded"
    exit 0
}

test "${msg#*strength*}" != "${msg}" || {
    echo "not ok" 1 - "missing stbn strength key diagnostic"
    exit 0
}

test "${msg#*0.0-2.0*}" != "${msg}" || {
    echo "not ok" 1 - "missing stbn strength range diagnostic"
    exit 0
}

echo "ok" 1 - "invalid stbn strength is rejected"
exit 0
