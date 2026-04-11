#!/bin/sh
# TAP test verifying unknown interframe strategy suboption values are rejected.

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
        -d interframe:strategy=unknown \
        "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
        -o /dev/null 2>&1
) && {
    echo "not ok" 1 - "unknown interframe strategy unexpectedly succeeded"
    exit 0
}

test "${msg#*unknown suboption value*}" != "${msg}" || {
    echo "not ok" 1 - "missing unknown interframe strategy diagnostic"
    exit 0
}

test "${msg#*strategy*}" != "${msg}" || {
    echo "not ok" 1 - "unknown interframe strategy diagnostic lacked key name"
    exit 0
}

echo "ok" 1 - "unknown interframe strategy value is rejected"
exit 0
