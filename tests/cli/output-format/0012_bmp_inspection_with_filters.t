#!/bin/sh
# Inspect BMP while applying palette and scaling filters.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

snake_bmp="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -I -v -w200 -hauto -c100x1000+40+20 -qlow -dnone         -rhamming -thls "${snake_bmp}" -o/dev/null || {
    echo "not ok" 1 - "BMP inspection with filters fails"
    exit 0
}

echo "ok" 1 - "BMP inspection with filters succeeds"

exit 0
