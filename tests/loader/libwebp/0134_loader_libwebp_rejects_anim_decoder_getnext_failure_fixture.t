#!/bin/sh
# TAP test confirming forced libwebp loader reports WebPAnimDecoderGetNext failure for decoder-stage corruption.

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
             "${TOP_SRCDIR}/tests/data/corrupted/bad_anim_decoder_getnext_failure.webp" \
             2>&1 >/dev/null) && {
    echo "not ok" 1 - "forced libwebp loader accepted decoder-getnext failure fixture"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${msg#*load_with_libwebp: WebPAnimDecoderGetNext failed.*}" != "${msg}" || {
    echo "not ok" 1 - "expected WebPAnimDecoderGetNext failure diagnostic was missing"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok" 1 - "forced libwebp loader reports WebPAnimDecoderGetNext failure for decoder-stage corruption"

exit 0
