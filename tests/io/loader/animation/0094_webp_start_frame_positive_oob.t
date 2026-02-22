#!/bin/sh
# TAP test: libwebp rejects out-of-range positive frame indexes.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

run_img2sixel --start-frame=999 -L libwebp! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-8x8-2frame-min.webp" \
    >/dev/null && {
    fail 1 "libwebp positive out-of-range start frame unexpectedly succeeded"
    exit 0
}

pass 1 "libwebp positive out-of-range start frame is rejected"
exit 0
