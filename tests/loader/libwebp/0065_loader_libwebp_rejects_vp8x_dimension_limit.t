#!/bin/sh
# TAP test confirming forced libwebp loader rejects VP8X dimensions over limit.

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
             "${TOP_SRCDIR}/tests/data/corrupted/bad_vp8x_dimensions_exceed_limit.webp" \
             2>&1 >/dev/null) && {
    echo "not ok" 1 - "forced libwebp loader accepted over-limit VP8X dimensions"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

case "${msg}" in
    *"webp decode: dimensions exceed limit."*)
        ;;
    *)
        echo "not ok" 1 - "expected dimensions-over-limit diagnostic was missing"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "forced libwebp loader rejects over-limit VP8X dimensions"

exit 0
