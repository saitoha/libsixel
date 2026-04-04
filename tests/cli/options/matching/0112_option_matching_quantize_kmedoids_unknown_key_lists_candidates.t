#!/bin/sh
# TAP test verifying unknown kmedoids suboption keys are rejected with candidates.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qkmedoids:unknown=1 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "unknown kmedoids suboption key unexpectedly succeeded"
    exit 0
}

test "${msg#*unknown suboption key*}" != "${msg}" || {
    echo "not ok" 1 - "missing unknown kmedoids key diagnostic"
    exit 0
}

test "${msg#*"unknown"*valid keys*algo*seed*iter*sample*}" != "${msg}" || {
    echo "not ok" 1 - "missing kmedoids key candidate list"
    exit 0
}

echo "ok" 1 - "unknown kmedoids suboption key is rejected with candidates"
exit 0
