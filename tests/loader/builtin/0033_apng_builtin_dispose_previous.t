#!/bin/sh
# TAP test: APNG dispose previous input is handled by builtin loader path.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

run_img2sixel -Lbuiltin! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_libpng_dispose_previous.png" \
    -o/dev/null || {
    echo "not ok" 1 "APNG dispose previous input failed"
    exit 0
}

echo "ok" 1 "APNG dispose previous input is handled"
exit 0
