#!/bin/sh
# Crop grayscale PNG using alternate ordered dither.
# Policy: docs/misc/platforms/openvms.md
# Coverage: OV-10
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

snake_gray_png="${TOP_SRCDIR}/images/snake-grayscale.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -c200x200+100+100 -w400 -da_dither \
        -o/dev/null "${snake_gray_png}" || {
    echo "not ok" 1 - "cropping with alternate dither fails"
    exit 0
}

echo "ok" 1 - "cropping with alternate dither succeeds"

exit 0
