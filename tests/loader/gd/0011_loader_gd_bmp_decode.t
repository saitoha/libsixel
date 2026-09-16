#!/bin/sh
# TAP test: gd loader decodes BMP input successfully.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


test "${HAVE_DECL_GDIMAGECREATEFROMBMPPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMBMPPTR is unavailable in this build\n";
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L gd! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-bmp3-rgb.bmp" \
    >/dev/null || {
    echo "not ok" 1 - "gd failed to decode BMP input"
    exit 0
}

echo "ok" 1 - "gd decodes BMP input"
exit 0
