#!/bin/sh
# TAP test confirming forced libwebp loader rejects odd-sized chunks without padding.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

run_img2sixel -L libwebp! "${TOP_SRCDIR}/tests/data/corrupted/bad_odd_chunk_missing_padding.webp" \
    >/dev/null && {
    echo "not ok" 1 - "forced libwebp loader accepted odd chunk without padding"
    exit 0
}

echo "ok" 1 - "forced libwebp loader rejects odd chunk without padding"

exit 0
