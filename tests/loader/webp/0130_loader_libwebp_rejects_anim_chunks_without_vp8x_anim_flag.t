#!/bin/sh
# TAP test confirming forced libwebp loader rejects ANIM/ANMF chunks when VP8X animation flag is unset.

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
             "${TOP_SRCDIR}/tests/data/corrupted/bad_anim_chunks_without_vp8x_anim_flag.webp" \
             2>&1 >/dev/null) && {
    echo "not ok" 1 - "forced libwebp loader accepted ANIM/ANMF chunks without VP8X animation flag"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

case "${msg}" in
    *"webp decode: ANIM/ANMF chunks require VP8X animation flag."*)
        ;;
    *)
        echo "not ok" 1 - "expected VP8X animation-flag requirement diagnostic was missing"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "forced libwebp loader rejects ANIM/ANMF chunks without VP8X animation flag"

exit 0
