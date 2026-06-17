#!/bin/sh
# TAP test verifying -Q rejects out-of-range sticky_weight values.

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
        -Qauto:sticky_weight=256 \
        "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
        -o /dev/null 2>&1
) && {
    echo "not ok" 1 - "out-of-range sticky_weight unexpectedly succeeded"
    exit 0
}

test "${msg#*-Q sticky_weight must be in range 0-255.*}" != "${msg}" || {
    echo "not ok" 1 - "missing out-of-range sticky_weight diagnostic"
    exit 0
}

echo "ok" 1 - "-Q rejects out-of-range sticky_weight values"
exit 0
