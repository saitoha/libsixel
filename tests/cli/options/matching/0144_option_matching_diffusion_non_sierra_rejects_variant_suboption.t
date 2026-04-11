#!/bin/sh
# TAP test verifying non-sierra diffusion rejects variant suboption.

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
        -d fs:variant=1 \
        "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
        -o /dev/null 2>&1
) && {
    echo "not ok" 1 - "non-sierra variant unexpectedly succeeded"
    exit 0
}

test "${msg#*unknown suboption key*}" != "${msg}" || {
    echo "not ok" 1 - "missing non-sierra variant diagnostic"
    exit 0
}

test "${msg#*variant*}" != "${msg}" || {
    echo "not ok" 1 - "non-sierra variant diagnostic lacked key name"
    exit 0
}

echo "ok" 1 - "non-sierra variant suboption is rejected"
exit 0
