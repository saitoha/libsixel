#!/bin/sh
# TAP test: quicklook reads MP4 as a still image.

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

test -n "${SIXEL_TEST_QUICKLOOK_MP4-}" || {
    printf "1..0 # SKIP set SIXEL_TEST_QUICKLOOK_MP4 to a decodable mp4 file\n"
    exit 0
}

echo "1..1"
set -v

run_img2sixel --env SIXEL_THUMBNAILER_HINT_SIZE=64 -L quicklook! \
    "${SIXEL_TEST_QUICKLOOK_MP4}" >/dev/null || {
    fail 1 "quicklook MP4 still-frame decode failed"
    exit 0
}

pass 1 "quicklook reads MP4 as still image"
exit 0
