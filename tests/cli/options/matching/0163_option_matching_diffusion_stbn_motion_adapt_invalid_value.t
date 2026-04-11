#!/bin/sh
# TAP test verifying invalid stbn motion_adapt value is rejected.

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
        -d stbn:motion_adapt=2 \
        "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
        -o /dev/null 2>&1
) && {
    echo "not ok" 1 - "invalid stbn motion_adapt unexpectedly passed"
    exit 0
}

test "${msg#*unknown suboption value*}" != "${msg}" || {
    echo "not ok" 1 - "missing stbn motion_adapt invalid-value diagnostic"
    exit 0
}

test "${msg#*motion_adapt*}" != "${msg}" || {
    echo "not ok" 1 - "motion_adapt invalid-value diagnostic lacked key name"
    exit 0
}

echo "ok" 1 - "invalid stbn motion_adapt value is rejected"
exit 0
