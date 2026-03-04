#!/bin/sh
# TAP test: libwebp -g ignores frame delay.

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

image_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-8x8-2frame-min.webp"

run_img2sixel -Llibwebp! -ldisable -g "${image_webp}" >/dev/null || {
    echo "not ok" 1 "libwebp ignore-delay failed"
    exit 0
}

echo "ok" 1 "libwebp ignore-delay succeeded"
exit 0
