#!/bin/sh
# TAP test confirming webp frame-limit guard trace reports both RIFF and decoder sources.

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

msg=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp:cms_engine=none! -l disable \
    "${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-8x8-2frame-min.webp" \
    2>&1 >/dev/null) || {
    echo "not ok" 1 - "animated WebP decode failed while collecting frame-guard trace"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${msg#*animation frame guard source=riff *}" != "${msg}" || {
    echo "not ok" 1 - "expected RIFF-side frame guard trace was missing"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${msg#*animation frame guard source=decoder *}" != "${msg}" || {
    echo "not ok" 1 - "expected decoder-side frame guard trace was missing"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok" 1 - "webp frame-limit guard trace reports both RIFF and decoder sources"

exit 0
