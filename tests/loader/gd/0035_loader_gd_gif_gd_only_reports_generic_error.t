#!/bin/sh
# TAP test: forced gd loader reports generic error for delegated GIF input.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_DECL_GDIMAGECREATEFROMPNGPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMPNGPTR is unavailable\n"
    exit 0
}

echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L gd! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" 2>&1 >/dev/null) && {
    echo "not ok" 1 - "forced gd loader unexpectedly accepted GIF input"
    exit 0
}

test "${msg#*runtime error: unable to decode input with available loaders*}" \
    != "${msg}" || {
    echo "not ok" 1 - "forced gd GIF failure missed generic decode error"
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${msg#*GD error*}" = "${msg}" || {
    echo "not ok" 1 - "forced gd GIF failure should not report GD error"
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok" 1 - "forced gd GIF failure reports delegated generic error"
exit 0
