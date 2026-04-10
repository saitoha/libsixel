#!/bin/sh
# TAP test: forced gd reports generic decode error for stdin interlaced PNG.

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

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L gd! -ldisable - \
    <"${TOP_SRCDIR}/tests/data/inputs/formats/snake-png-adam7-rgb.png" \
    2>&1 >/dev/null) && {
    echo "not ok" 1 - "forced gd loader unexpectedly accepted stdin interlaced PNG"
    exit 0
}

test "${msg#*runtime error: unable to decode input with available loaders*}" \
    != "${msg}" || {
    echo "not ok" 1 - "forced gd stdin interlaced PNG missed generic decode error"
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${msg#*GD error*}" = "${msg}" || {
    echo "not ok" 1 - "forced gd stdin interlaced PNG should not report GD error"
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok" 1 - "forced gd stdin interlaced PNG reports delegated generic error"
exit 0
