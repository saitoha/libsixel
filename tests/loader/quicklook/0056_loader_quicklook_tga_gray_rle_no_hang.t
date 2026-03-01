#!/bin/sh
# TAP test: quicklook does not hang on grayscale RLE TGA input.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_QUICKLOOK-}" = 1 || {
    printf "1..0 # SKIP quicklook loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

run_img2sixel --env SIXEL_THUMBNAILER_HINT_SIZE=64 -L quicklook! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-tga-type11-gray.tga" \
    >/dev/null || {
    echo "not ok" 1 "quicklook grayscale RLE TGA decode failed"
    exit 0
}

echo "ok" 1 "quicklook does not hang on grayscale RLE TGA"
exit 0
