#!/bin/sh
# TAP test: gd loader decodes BMP input successfully.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_DECL_GDIMAGECREATEFROMBMPPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMBMPPTR is unavailable in this build\n";
    exit 0
}

echo "1..1"
set -v

run_img2sixel -L gd! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-bmp3-rgb.bmp" \
    >/dev/null || {
    fail 1 "gd failed to decode BMP input"
    exit 0
}

pass 1 "gd decodes BMP input"
exit 0
