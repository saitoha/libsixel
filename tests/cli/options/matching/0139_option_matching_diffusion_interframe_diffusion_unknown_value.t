#!/bin/sh
# TAP test verifying unknown interframe diffusion value is rejected.

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
        --env SIXEL_DIAG_MODE=code \
        --env SIXEL_DIAG_MODE_QUIET=1 \
        -d interframe:diffusion=invalid-kernel \
        "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
        -o /dev/null 2>&1
) && {
    echo "not ok" 1 - "unknown interframe diffusion value unexpectedly passed"
    exit 0
}

test "${msg#*unknown suboption value*}" != "${msg}" || {
    echo "not ok" 1 - "missing interframe diffusion unknown-value diagnostic"
    exit 0
}

test "${msg#*diffusion*}" != "${msg}" || {
    echo "not ok" 1 - "interframe diffusion diagnostic lacked key name"
    exit 0
}

echo "ok" 1 - "unknown interframe diffusion value is rejected"
exit 0
