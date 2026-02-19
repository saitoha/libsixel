#!/bin/sh
# TAP test: libwebp decodes tiny lossless animation.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

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

image_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-8x8-2frame-min.webp"

run_img2sixel -Llibwebp -ldisable "${image_webp}" >/dev/null || {
    fail 1 "libwebp lossless animation decode failed"
    exit 0
}

pass 1 "libwebp lossless animation decode succeeded"
exit 0
