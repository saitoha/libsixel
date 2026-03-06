#!/bin/sh
# TAP test: gd loader decodes TGA input successfully.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


test "${HAVE_DECL_GDIMAGECREATEFROMTGAPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMTGAPTR is unavailable in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"
set -v

run_img2sixel -L gd! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-tga-type2-rgb.tga" \
    >/dev/null || {
    echo "not ok" 1 - "gd failed to decode TGA input"
    exit 0
}

echo "ok" 1 - "gd decodes TGA input"
exit 0
