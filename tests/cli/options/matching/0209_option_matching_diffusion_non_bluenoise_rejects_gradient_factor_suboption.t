#!/bin/sh
# TAP test verifying non-bluenoise diffusion rejects gradient_factor key.

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
        -d fs:gradient_factor=1.0 \
        "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
        -o /dev/null 2>&1
) && {
    echo "not ok" 1 - "non-bluenoise gradient_factor unexpectedly passed"
    exit 0
}

test "${msg#*unknown suboption key*}" != "${msg}" || {
    echo "not ok" 1 - "missing non-bluenoise gradient_factor diagnostic"
    exit 0
}

test "${msg#*gradient_factor*}" != "${msg}" || {
    echo "not ok" 1 - "diagnostic lacked gradient_factor key name"
    exit 0
}

echo "ok" 1 - "non-bluenoise gradient_factor suboption is rejected"
exit 0
