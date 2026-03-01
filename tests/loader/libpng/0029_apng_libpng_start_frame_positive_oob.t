#!/bin/sh
# TAP test: libpng APNG positive out-of-range start frame returns an error.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

run_img2sixel --start-frame=999 \
    -Llibpng! -S \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgb_loop2.png" \
    >/dev/null && {
    echo "not ok" 1 "out-of-range positive start frame unexpectedly succeeded"
    exit 0
}

echo "ok" 1 "libpng APNG positive out-of-range start frame is rejected"
exit 0
