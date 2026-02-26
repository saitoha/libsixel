#!/bin/sh
# TAP test: quicklook reads animated GIF as a still image.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_QUICKLOOK-}" = 1 || {
    printf "1..0 # SKIP quicklook loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

run_img2sixel --env SIXEL_THUMBNAILER_HINT_SIZE=64 -L quicklook! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-gif-interlaced.gif" >/dev/null || {
    fail 1 "quicklook GIF still-frame decode failed"
    exit 0
}

pass 1 "quicklook reads GIF as still image"
exit 0
