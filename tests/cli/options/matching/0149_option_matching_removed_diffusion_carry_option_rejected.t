#!/bin/sh
# TAP test verifying removed -Y diffusion carry option is rejected.

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
        -Y carry \
        "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
        -o /dev/null 2>&1
) && {
    echo "not ok" 1 - "removed -Y option unexpectedly succeeded"
    exit 0
}

test "${msg#*invalid option*}" != "${msg}" || {
    echo "not ok" 1 - "missing invalid-option diagnostic for removed -Y"
    exit 0
}

echo "ok" 1 - "removed -Y option is rejected"
exit 0
