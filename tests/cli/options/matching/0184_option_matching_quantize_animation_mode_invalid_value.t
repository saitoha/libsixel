#!/bin/sh
# TAP test verifying -Q rejects animation_mode values other than 0 or 1.

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
        -Qauto:animation_mode=2 \
        "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
        -o /dev/null 2>&1
) && {
    echo "not ok" 1 - "invalid animation_mode unexpectedly succeeded"
    exit 0
}

test "${msg#*-Q animation_mode must be 0 or 1.*}" != "${msg}" || {
    echo "not ok" 1 - "missing invalid animation_mode diagnostic"
    exit 0
}

echo "ok" 1 - "-Q rejects invalid animation_mode values"
exit 0
