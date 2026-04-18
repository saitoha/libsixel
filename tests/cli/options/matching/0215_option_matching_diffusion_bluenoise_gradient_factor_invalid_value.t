#!/bin/sh
# TAP test verifying invalid bluenoise gradient_factor values are rejected.

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
        -d bluenoise:gradient_factor=abc \
        "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
        -o /dev/null 2>&1
) && {
    echo "not ok" 1 - "invalid bluenoise gradient_factor unexpectedly passed"
    exit 0
}

test "${msg#*-d bluenoise:gradient_factor must be a floating point value.*}" \
    != "${msg}" || {
    echo "not ok" 1 - "missing bluenoise gradient_factor diagnostic"
    exit 0
}

echo "ok" 1 - "invalid bluenoise gradient_factor is rejected"
exit 0
