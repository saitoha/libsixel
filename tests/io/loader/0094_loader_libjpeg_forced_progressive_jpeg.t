#!/bin/sh
# TAP test confirming --loaders libjpeg! decodes progressive JPEG input.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

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

input_jpeg="${TOP_SRCDIR}/images/snake-progressive-16x16.jpg"

run_img2sixel -L libjpeg! "${input_jpeg}" >/dev/null || {
    fail 1 "libjpeg forced progressive JPEG decoding failed"
    exit 0
}

pass 1 "libjpeg forced progressive JPEG decoding succeeds"
exit 0
