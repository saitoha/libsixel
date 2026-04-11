#!/bin/sh
# TAP test verifying unknown stbn suboption keys are rejected.

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
        -d stbn:mode=hash \
        "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
        -o /dev/null 2>&1
) && {
    echo "not ok" 1 - "unknown stbn key unexpectedly succeeded"
    exit 0
}

test "${msg#*unknown suboption key*}" != "${msg}" || {
    echo "not ok" 1 - "missing unknown stbn key diagnostic"
    exit 0
}

test "${msg#*source*}" != "${msg}" || {
    echo "not ok" 1 - "unknown key diagnostic lacked valid source key hint"
    exit 0
}

echo "ok" 1 - "unknown stbn key is rejected"
exit 0
