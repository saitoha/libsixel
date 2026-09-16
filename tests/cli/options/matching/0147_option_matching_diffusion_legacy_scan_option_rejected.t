#!/bin/sh
# TAP test verifying legacy -y diffusion scan option is rejected.

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
        -y raster \
        "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
        -o /dev/null 2>&1
) && {
    echo "not ok" 1 - "legacy -y scan option unexpectedly succeeded"
    exit 0
}

test "${msg#*invalid option*}" != "${msg}" && {
    echo "ok" 1 - "legacy -y scan option is rejected"
    exit 0
}

test "${msg#*unknown option*}" != "${msg}" && {
    echo "ok" 1 - "legacy -y scan option is rejected"
    exit 0
}

test "${msg#*illegal option*}" != "${msg}" && {
    echo "ok" 1 - "legacy -y scan option is rejected"
    exit 0
}

test "${msg#*unrecognized option*}" != "${msg}" && {
    echo "ok" 1 - "legacy -y scan option is rejected"
    exit 0
}

echo "not ok" 1 - "missing option-rejection diagnostic for legacy -y"
exit 0
