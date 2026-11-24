#!/bin/sh
# TAP test confirming libjpeg loader rejects non-JPEG signatures.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


test "${HAVE_JPEG-}" = 1 || {
    printf "1..0 # SKIP libjpeg loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/rgba.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libjpeg! "${input_png}" >/dev/null && {
    printf '\n%s\n' "not ok 1 - non-JPEG data unexpectedly accepted by libjpeg"
    exit 0
}

printf '\n%s\n' "ok 1 - non-JPEG data is rejected by libjpeg"
exit 0
