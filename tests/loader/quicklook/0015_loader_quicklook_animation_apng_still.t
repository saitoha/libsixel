#!/bin/sh
# TAP test: quicklook reads APNG as a still image.

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

run_img2sixel --env SIXEL_TRACE_TOPIC=encode_handoff,apng_decode,lifecycle --env SIXEL_THUMBNAILER_HINT_SIZE=64 -L quicklook! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgb_loop2.png" >/dev/null || {
    echo "not ok" 1 "quicklook APNG still-frame decode failed"
    exit 0
}

echo "ok" 1 "quicklook reads APNG as still image"
exit 0
