#!/bin/sh
# TAP test verifying unknown orientation suboption values are rejected.

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
    echo "not ok" 1 - "unknown orientation suboption value unexpectedly succeeded"
    exit 0
}

test "${msg#*unknown suboption value*}" != "${msg}" || {
    echo "not ok" 1 - "missing unknown orientation suboption diagnostic"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${msg#*\"yes\"*}" != "${msg}" || {
    echo "not ok" 1 - "unknown orientation value token not reported"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${msg#*\"orientation\"*}" != "${msg}" || {
    echo "not ok" 1 - "orientation key token not reported"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${msg#*valid values*}" != "${msg}" || {
    echo "not ok" 1 - "valid-values hint missing for orientation suboption"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${msg#*on*off*}" != "${msg}" || {
    echo "not ok" 1 - "orientation valid values did not list on/off"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok" 1 - "unknown orientation suboption value is rejected"
exit 0
