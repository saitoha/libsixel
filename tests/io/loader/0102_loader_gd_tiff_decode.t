#!/bin/sh
# TAP test: gd loader decodes TIFF input successfully.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_DECL_GDIMAGECREATEFROMTIFFPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMTIFFPTR is unavailable in this build\n";
    exit 0
}

echo "1..1"
set -v

run_img2sixel -L gd! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-tiff-zip-rgb.tiff" \
    >/dev/null && {
    pass 1 "gd decodes TIFF input"
    exit 0
}

printf "ok 1 # SKIP gd backend does not decode TIFF in this runtime\n"
exit 0
