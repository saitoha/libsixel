#!/bin/sh
# TAP test verifying invalid interframe noise_strength values are rejected.

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
        -d interframe:noise_strength=invalid \
        "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
        -o /dev/null 2>&1
) && {
    echo "not ok" 1 - "invalid interframe noise_strength unexpectedly succeeded"
    exit 0
}

test "${msg#*noise_strength*}" != "${msg}" || {
    echo "not ok" 1 - "missing interframe noise_strength key diagnostic"
    exit 0
}

test "${msg#*0.0-2.0*}" != "${msg}" || {
    echo "not ok" 1 - "missing interframe noise_strength range diagnostic"
    exit 0
}

echo "ok" 1 - "invalid interframe noise_strength is rejected"
exit 0
