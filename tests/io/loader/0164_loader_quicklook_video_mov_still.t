#!/bin/sh
# TAP test: quicklook reads MOV as a still image.

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

test -n "${SIXEL_TEST_QUICKLOOK_MOV-}" || {
    printf "1..0 # SKIP set SIXEL_TEST_QUICKLOOK_MOV to a decodable mov file\n"
    exit 0
}

echo "1..1"
set -v

run_img2sixel --env SIXEL_THUMBNAILER_HINT_SIZE=64 -L quicklook! \
    "${SIXEL_TEST_QUICKLOOK_MOV}" >/dev/null || {
    fail 1 "quicklook MOV still-frame decode failed"
    exit 0
}

pass 1 "quicklook reads MOV as still image"
exit 0
