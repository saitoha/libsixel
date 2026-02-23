#!/bin/sh
# TAP test confirming libjpeg rejects corrupted JPEG streams.

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

input_jpeg="${TOP_SRCDIR}/tests/data/corrupted/metadata_noise.jpg"

run_img2sixel -L libjpeg! "${input_jpeg}" >/dev/null && {
    fail 1 "corrupted JPEG unexpectedly decoded"
    exit 0
}

pass 1 "corrupted JPEG is rejected"
exit 0
