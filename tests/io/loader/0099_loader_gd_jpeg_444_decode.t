#!/bin/sh
# TAP test: gd loader decodes 4:4:4 JPEG input successfully.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_DECL_GDIMAGECREATEFROMJPEGPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMJPEGPTR is unavailable in this build\n";
    exit 0
}

echo "1..1"
set -v

run_img2sixel -L gd! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-jpeg-444.jpg" \
    >/dev/null || {
    fail 1 "gd failed to decode 4:4:4 JPEG input"
    exit 0
}

pass 1 "gd decodes 4:4:4 JPEG input"
exit 0
