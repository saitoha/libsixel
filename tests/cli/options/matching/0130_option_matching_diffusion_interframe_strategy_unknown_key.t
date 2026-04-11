#!/bin/sh
# TAP test verifying unknown interframe strategy keys are rejected.

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
        -d interframe:mode=stbn-hash \
        "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
        -o /dev/null 2>&1
) && {
    echo "not ok" 1 - "unknown interframe strategy key unexpectedly succeeded"
    exit 0
}

test "${msg#*unknown suboption key*}" != "${msg}" || {
    echo "not ok" 1 - "missing unknown interframe strategy key diagnostic"
    exit 0
}

test "${msg#*strategy*}" != "${msg}" || {
    echo "not ok" 1 - "unknown key diagnostic lacked valid key hint"
    exit 0
}

echo "ok" 1 - "unknown interframe strategy key is rejected"
exit 0
