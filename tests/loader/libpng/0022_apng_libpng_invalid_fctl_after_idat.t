#!/bin/sh
# The fixture incorrectly counts its excluded default PNG as a frame.
# TAP test: APNG fcTL after first IDAT input overcount is rejected by libpng path.

set -eux

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

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_TRACE_TOPIC=encode_handoff,apng_decode,lifecycle  \
    -v -Llibpng! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_invalid_libpng_fctl_after_idat.png" \
    -o/dev/null && {
    echo "not ok" 1 - "APNG fcTL overcount was accepted"
    exit 0
}

echo "ok" 1 - "APNG fcTL after IDAT overcount is rejected"
exit 0
