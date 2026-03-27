#!/bin/sh
# TAP test confirming --loaders libjpeg! forces JPEG decoding path.

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

input_jpeg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libjpeg! "${input_jpeg}" >/dev/null || {
    echo "not ok" 1 - "libjpeg forced JPEG decoding failed"
    exit 0
}

echo "ok" 1 - "libjpeg forced JPEG decoding succeeds"
exit 0
