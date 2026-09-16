#!/bin/sh
# TAP test verifying invalid sierra variant value is rejected.

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
        -d sierra:variant=invalid \
        "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
        -o /dev/null 2>&1
) && {
    echo "not ok" 1 - "invalid sierra variant unexpectedly succeeded"
    exit 0
}

test "${msg#*unknown suboption value*}" != "${msg}" || {
    echo "not ok" 1 - "missing invalid sierra variant diagnostic"
    exit 0
}

test "${msg#*variant*}" != "${msg}" || {
    echo "not ok" 1 - "invalid sierra variant diagnostic lacked key name"
    exit 0
}

echo "ok" 1 - "invalid sierra variant value is rejected"
exit 0
