#!/bin/sh
# TAP test verifying unknown builtin bmp_info40_mode value is rejected.

set -eux


test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Lbuiltin:bmp_info40_mode=invalid! \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" -o/dev/null 2>&1) && {
    echo "not ok" 1 - "unknown builtin bmp_info40_mode value unexpectedly succeeded"
    exit 0
}

test "${msg#*unknown suboption value*}" != "${msg}" || {
    echo "not ok" 1 - "missing unknown suboption value diagnostic"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${msg#*\"bmp_info40_mode\"*}" != "${msg}" || {
    echo "not ok" 1 - "missing bmp_info40_mode token in diagnostic"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${msg#*auto*}" != "${msg}" || {
    echo "not ok" 1 - "valid-values list missing auto"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${msg#*windows*}" != "${msg}" || {
    echo "not ok" 1 - "valid-values list missing windows"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${msg#*os2*}" != "${msg}" || {
    echo "not ok" 1 - "valid-values list missing os2"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok" 1 - "unknown builtin bmp_info40_mode value is rejected"
exit 0
