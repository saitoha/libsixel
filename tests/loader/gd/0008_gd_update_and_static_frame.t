#!/bin/sh
# TAP test: gd loader accepts combined update and static frame options.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


test "${HAVE_DECL_GDIMAGECREATEFROMGIFANIMPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMGIFANIMPTR is unavailable in this build\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L gd! -ldisable -dnone -u -g \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" >/dev/null || {
    echo "not ok" 1 - "gd combined update/static frame failed"
    exit 0
}

echo "ok" 1 - "gd combined update/static frame succeeded"
exit 0
