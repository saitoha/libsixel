#!/bin/sh
# TAP test confirming forced libwebp loader reports WebPDecodeRGBAInto failure for static alpha decode corruption.

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

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp:cms_engine=none! \
             "${TOP_SRCDIR}/tests/data/corrupted/bad_static_decode_rgbainto_failure.webp" \
             2>&1 >/dev/null) && {
    echo "not ok" 1 - "forced libwebp loader accepted static RGBA decode-failure fixture"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${msg#*load_webp: WebPDecodeRGBAInto failed.*}" != "${msg}" || {
    echo "not ok" 1 - "expected WebPDecodeRGBAInto failure diagnostic was missing"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok" 1 - "forced libwebp loader reports WebPDecodeRGBAInto failure for static alpha decode corruption"

exit 0
