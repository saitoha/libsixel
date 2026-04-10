#!/bin/sh
# TAP test verifying non-temporal diffusion rejects strategy suboptions.

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
        -d fs:strategy=stbn-mask \
        "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
        -o /dev/null 2>&1
) && {
    echo "not ok" 1 - "non-temporal diffusion strategy unexpectedly succeeded"
    exit 0
}

test "${msg#*supported only for temporal-diffusion*}" != "${msg}" || {
    echo "not ok" 1 - "missing non-temporal strategy rejection diagnostic"
    exit 0
}

echo "ok" 1 - "non-temporal diffusion strategy suboption is rejected"
exit 0
