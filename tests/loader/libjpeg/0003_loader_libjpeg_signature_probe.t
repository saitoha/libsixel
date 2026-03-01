#!/bin/sh
# TAP test confirming libjpeg loader rejects non-JPEG signatures.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_JPEG-}" = 1 || {
    printf "1..0 # SKIP libjpeg loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/rgba.png"

run_img2sixel -L libjpeg! "${input_png}" >/dev/null && {
    echo "not ok" 1 "non-JPEG data unexpectedly accepted by libjpeg"
    exit 0
}

echo "ok" 1 "non-JPEG data is rejected by libjpeg"
exit 0
