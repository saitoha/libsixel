#!/bin/sh
# TAP test: libwebp -l disable suppresses loop playback.

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

image_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-8x8-2frame-loop2-min.webp"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp! -ldisable "${image_webp}" >/dev/null || {
    echo "not ok" 1 - "libwebp loop disable failed"
    exit 0
}

echo "ok" 1 - "libwebp loop disable succeeded"
exit 0
