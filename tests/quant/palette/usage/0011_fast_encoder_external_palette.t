#!/bin/sh
# Validate fast encoder when using an external palette.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

snake_ppm="${TOP_SRCDIR}/tests/data/inputs/snake_64.ppm"
map8_palette="${TOP_SRCDIR}/images/map8-palette.png"

run_img2sixel -8 -m "${map8_palette}" -Esize "${snake_ppm}" \
        -o/dev/null || {
    echo "not ok" 1 - "fast encoder with palette fails"
    exit 0
}

echo "ok" 1 - "fast encoder with palette succeeds"

exit 0
