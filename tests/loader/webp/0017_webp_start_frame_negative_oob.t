#!/bin/sh
# TAP test: libwebp rejects out-of-range negative frame indexes.

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

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --start-frame=-999 -L libwebp! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-8x8-2frame-min.webp" \
    >/dev/null && {
    echo "not ok" 1 - "libwebp negative out-of-range start frame unexpectedly succeeded"
    exit 0
}

echo "ok" 1 - "libwebp negative out-of-range start frame is rejected"
exit 0
