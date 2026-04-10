#!/bin/sh
# TAP test verifying non-interframe diffusion rejects noise_strength key.

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
        -d fs:noise_strength=0.50 \
        "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
        -o /dev/null 2>&1
) && {
    echo "not ok" 1 - "non-interframe noise_strength unexpectedly succeeded"
    exit 0
}

test "${msg#*supported only for interframe*}" != "${msg}" || {
    echo "not ok" 1 - "missing non-interframe noise_strength diagnostic"
    exit 0
}

echo "ok" 1 - "non-interframe noise_strength suboption is rejected"
exit 0
