#!/bin/sh
# TAP test: APNG fcTL after first IDAT input input is handled by libpng path.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

run_img2sixel --env SIXEL_TRACE_TOPIC=apng_decode -Llibpng! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_invalid_libpng_fctl_after_idat.png" \
    -o/dev/null || {
    fail 1 "APNG fcTL after IDAT failed"
    exit 0
}

pass 1 "APNG fcTL after IDAT input is handled"
exit 0
