#!/bin/sh
# TAP test: APNG fcTL after first IDAT input input is handled by builtin loader path.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

run_img2sixel -Lbuiltin! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_invalid_libpng_fctl_after_idat.png" \
    -o/dev/null || {
    fail 1 "APNG fcTL after IDAT failed"
    exit 0
}

pass 1 "APNG fcTL after IDAT input is handled"
exit 0
