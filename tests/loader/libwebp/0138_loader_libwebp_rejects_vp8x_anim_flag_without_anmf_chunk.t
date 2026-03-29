#!/bin/sh
# TAP test confirming forced libwebp loader rejects VP8X animation streams that carry ANIM but no ANMF.

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

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp:cms_engine=none! -l disable \
             "${TOP_SRCDIR}/tests/data/corrupted/bad_vp8x_anim_flag_without_anmf_chunk.webp" \
             2>&1 >/dev/null) && {
    echo "not ok" 1 - "forced libwebp loader accepted VP8X animation stream without ANMF"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

case "${msg}" in
    *"webp decode: VP8X animation flag requires ANIM and ANMF chunks."*)
        ;;
    *)
        echo "not ok" 1 - "expected VP8X ANIM/ANMF consistency diagnostic was missing"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "forced libwebp loader rejects VP8X animation streams with ANIM but without ANMF"

exit 0
