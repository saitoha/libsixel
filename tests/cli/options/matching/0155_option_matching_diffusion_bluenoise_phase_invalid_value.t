#!/bin/sh
# TAP test verifying invalid bluenoise phase values are rejected.

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
        -d bluenoise:phase=3 \
        "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
        -o /dev/null 2>&1
) && {
    echo "not ok" 1 - "invalid bluenoise phase unexpectedly succeeded"
    exit 0
}

test "${msg#*phase*}" != "${msg}" || {
    echo "not ok" 1 - "missing bluenoise phase key diagnostic"
    exit 0
}

test "${msg#*X,Y*}" != "${msg}" || {
    echo "not ok" 1 - "missing bluenoise phase format diagnostic"
    exit 0
}

echo "ok" 1 - "invalid bluenoise phase is rejected"
exit 0
