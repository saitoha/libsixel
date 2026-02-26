#!/bin/sh
# TAP test: quicklook probe for WebM still-image decode.

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

test -n "${SIXEL_TEST_QUICKLOOK_WEBM-}" || {
    printf "1..0 # SKIP set SIXEL_TEST_QUICKLOOK_WEBM to a decodable webm file\n"
    exit 0
}

echo "1..1"
set -v

run_img2sixel --env SIXEL_THUMBNAILER_HINT_SIZE=64 -L quicklook! \
    "${SIXEL_TEST_QUICKLOOK_WEBM}" >/dev/null || {
    fail 1 "quicklook WebM still-frame decode failed"
    exit 0
}

pass 1 "quicklook probe reads WebM as still image"
exit 0
