#!/bin/sh
# TAP test verifying legacy -d temporal-diffusion is rejected.

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
        -d temporal-diffusion \
        "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
        -o /dev/null 2>&1
) && {
    echo "not ok" 1 - "legacy temporal-diffusion unexpectedly succeeded"
    exit 0
}

test "${msg#*interframe*}" != "${msg}" || {
    echo "not ok" 1 - "legacy rejection diagnostic missed interframe hint"
    exit 0
}

echo "ok" 1 - "legacy temporal-diffusion is rejected"
exit 0
