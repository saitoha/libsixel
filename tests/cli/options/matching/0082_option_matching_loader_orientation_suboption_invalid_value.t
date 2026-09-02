#!/bin/sh
# TAP test verifying orientation uses the shared boolean validator.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibpng:orientation=yes! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/orientation_exif_o6_12x8.png" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "invalid orientation suboption value succeeded"
    exit 0
}

test "${msg#*boolean suboption must be 0 or 1.*}" != "${msg}" || {
    echo "not ok" 1 - "missing shared boolean diagnostic"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${msg#*libpng:orientation=yes!*}" != "${msg}" || {
    echo "not ok" 1 - "unknown orientation value token not reported"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${msg#*:orientation=0|1*}" != "${msg}" || {
    echo "not ok" 1 - "orientation key token not reported"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok" 1 - "orientation uses the shared boolean validator"
exit 0
