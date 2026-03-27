#!/bin/sh
# TAP test confirming forced libwebp loader rejects containers whose first chunk is not VP8/VP8L/VP8X.

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

msg=$(set +xv; run_img2sixel -L libwebp! \
             "${TOP_SRCDIR}/tests/data/corrupted/bad_first_chunk_not_vp8.webp" \
             2>&1 >/dev/null) && {
    echo "not ok" 1 - "forced libwebp loader accepted invalid first chunk"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

case "${msg}" in
    *"webp decode: first chunk must be VP8/VP8L/VP8X."*)
        ;;
    *)
        echo "not ok" 1 - "expected first-chunk diagnostic was missing"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "forced libwebp loader rejects invalid first chunk"

exit 0
