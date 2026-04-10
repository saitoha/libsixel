#!/bin/sh
# TAP test: APNG from stdin fails on gd-only and succeeds with builtin fallback.

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
    <"${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgb_loop2.png" \
    2>&1 >/dev/null) && {
    echo "not ok" 1 - "forced gd loader unexpectedly accepted APNG stdin"
    exit 0
}

test "${msg#*runtime error: unable to decode input with available loaders*}" \
    != "${msg}" || {
    echo "not ok" 1 - "forced gd APNG stdin failure missed generic decode error"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L gd,builtin! -ldisable - \
    <"${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgb_loop2.png" \
    >/dev/null || {
    echo "not ok" 1 - "gd,builtin failed to decode APNG from stdin"
    exit 0
}

echo "ok" 1 - "APNG stdin delegates from gd to builtin"
exit 0
