#!/bin/sh
# TAP test confirming libtiff loader rejects non-TIFF signatures.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_LIBTIFF-}" = 1 || {
    printf "1..0 # SKIP libtiff support is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/rgba.png"

run_img2sixel -L libtiff! "${input_png}" >/dev/null && {
    echo "not ok" 1 "non-TIFF data unexpectedly accepted by libtiff"
    exit 0
}

echo "ok" 1 "non-TIFF data is rejected by libtiff"
exit 0
