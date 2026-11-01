#!/bin/sh
# The fixture incorrectly counts its excluded default PNG as a frame.
# TAP test: APNG default image first overcount is rejected on libpng path.

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

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_TRACE_TOPIC=encode_handoff,apng_decode,lifecycle -Llibpng! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_libpng_default_image_first_valid.png" \
    -o/dev/null && {
    echo "not ok" 1 - "APNG default image first overcount was accepted"
    exit 0
}

echo "ok" 1 - "APNG default image first overcount is rejected"
exit 0
