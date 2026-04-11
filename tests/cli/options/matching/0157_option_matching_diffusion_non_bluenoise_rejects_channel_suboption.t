#!/bin/sh
# TAP test verifying non-bluenoise diffusion rejects channel key.

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
        -d fs:channel=rgb \
        "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
        -o /dev/null 2>&1
) && {
    echo "not ok" 1 - "non-bluenoise channel suboption unexpectedly passed"
    exit 0
}

test "${msg#*unknown suboption key*}" != "${msg}" || {
    echo "not ok" 1 - "missing non-bluenoise channel diagnostic"
    exit 0
}

test "${msg#*channel*}" != "${msg}" || {
    echo "not ok" 1 - "non-bluenoise channel diagnostic lacked key name"
    exit 0
}

echo "ok" 1 - "non-bluenoise channel suboption is rejected"
exit 0
