#!/bin/sh
# TAP test verifying unknown -d diffusion base values are rejected.

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
        -d definitely-unknown-diffusion \
        "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
        -o /dev/null 2>&1
) && {
    echo "not ok" 1 - "unknown diffusion value unexpectedly succeeded"
    exit 0
}

test "${msg#*unknown*}" != "${msg}" || {
    echo "not ok" 1 - "missing unknown diffusion diagnostic"
    exit 0
}

echo "ok" 1 - "unknown diffusion value is rejected"
exit 0
