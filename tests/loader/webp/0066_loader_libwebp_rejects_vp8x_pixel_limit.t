#!/bin/sh
# TAP test confirming forced libwebp loader rejects VP8X dimensions over pixel budget.

set -eux

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

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! \
             "${TOP_SRCDIR}/tests/data/corrupted/bad_vp8x_image_exceeds_pixel_limit.webp" \
             2>&1 >/dev/null) && {
    printf '\n%s\n' "not ok 1 - forced libwebp loader accepted over-budget VP8X pixel count"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

case "${msg}" in
    *"webp decode: image exceeds pixel limit."*)
        ;;
    *)
        printf '\n%s\n' "not ok 1 - expected pixel-limit diagnostic was missing"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

printf '\n%s\n' "ok 1 - forced libwebp loader rejects over-budget VP8X pixel count"

exit 0
