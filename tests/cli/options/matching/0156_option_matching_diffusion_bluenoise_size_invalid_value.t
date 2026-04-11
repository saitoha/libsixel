#!/bin/sh
# TAP test verifying invalid bluenoise size values are rejected.

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
        -d bluenoise:size=63 \
        "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
        -o /dev/null 2>&1
) && {
    echo "not ok" 1 - "invalid bluenoise size unexpectedly succeeded"
    exit 0
}

test "${msg#*unknown suboption value*}" != "${msg}" || {
    echo "not ok" 1 - "missing bluenoise size diagnostic prefix"
    exit 0
}

test "${msg#*size*}" != "${msg}" || {
    echo "not ok" 1 - "missing bluenoise size key diagnostic"
    exit 0
}

echo "ok" 1 - "invalid bluenoise size is rejected"
exit 0
