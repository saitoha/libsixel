#!/bin/sh
# TAP test for 12-bit JPEG decode path in forced libjpeg loader mode.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_JPEG-}" = 1 || {
    printf "1..0 # SKIP libjpeg loader is unavailable\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"
set -v

input_jpeg="${TOP_SRCDIR}/tests/data/inputs/formats/snake-jpeg-12bit.jpg"

if test "${HAVE_JPEG12_API-}" = 1; then
    run_img2sixel -L libjpeg! "${input_jpeg}" >/dev/null || {
        echo "not ok" 1 - "forced libjpeg 12-bit JPEG decode failed"
        exit 0
    }
    echo "ok" 1 - "forced libjpeg 12-bit JPEG decode succeeds"
    exit 0
fi

run_img2sixel -L libjpeg! "${input_jpeg}" >/dev/null && {
    echo "not ok" 1 - "forced libjpeg accepted 12-bit JPEG without API support"
    exit 0
}

echo "ok" 1 - "forced libjpeg rejects 12-bit JPEG when API is unavailable"
exit 0
