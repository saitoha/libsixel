#!/bin/sh
# Validate TIFF conversion with palette and filter options.
set -eux

test "${HAVE_LIBTIFF-}" = 1 || {
    printf "1..0 # SKIP libtiff support is disabled in this build\n";
    exit 0
}


test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

snake_tiff="${TOP_SRCDIR}/tests/data/inputs/snake_64.tiff"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibtiff! -p200 -8 -scenter -Brgb:0/f/A -h100 -qfull -rhan -dstucki \
    -thls "${snake_tiff}" -o/dev/null || {
    echo "not ok" 1 - "TIFF conversion with palette controls failed"
    exit 0
}

echo "ok" 1 - "TIFF conversion with palette controls succeeded"

exit 0
