#!/bin/sh
# TAP test confirming builtin WebP loop auto stops for finite animation loops.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-8x8-2frame-loop2-min.webp"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! -lauto "${input_webp}" >/dev/null || {
    echo "not ok" 1 - "builtin WebP animation loop auto failed"
    exit 0
}

echo "ok" 1 - "builtin WebP animation loop auto finished for finite loop"
exit 0
