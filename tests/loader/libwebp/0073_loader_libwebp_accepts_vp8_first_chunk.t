#!/bin/sh
# TAP test: libwebp accepts a valid RIFF VP8 first chunk.

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
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.webp" \
    2>&1 >/dev/null) || {
    echo "not ok" 1 - "valid VP8 first chunk was rejected"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok" 1 - "valid VP8 first chunk is accepted"

exit 0
