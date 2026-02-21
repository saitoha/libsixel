#!/bin/sh
# TAP test: APNG default image first input is handled on builtin loader path.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

run_img2sixel -Lbuiltin! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_libpng_default_image_first_valid.png" \
    -o/dev/null || {
    fail 1 "APNG default image first input failed"
    exit 0
}

pass 1 "APNG default image first input is handled"
exit 0
