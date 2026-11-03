#!/bin/sh
# Inspect grayscale JPEG without errors.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

snake_gray_jpg="${TOP_SRCDIR}/images/snake-grayscale.jpg"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -I "${snake_gray_jpg}" -o/dev/null || {
    echo "not ok" 1 - "grayscale JPEG inspection fails"
    exit 0
}

echo "ok" 1 - "grayscale JPEG inspection succeeds"

exit 0
