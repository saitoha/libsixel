#!/bin/sh
# TAP test confirming forced librsvg loader rejects non-SVG signatures.

set -eux

test "${HAVE_LIBRSVG-}" = 1 || {
    printf "1..0 # SKIP librsvg loader is unavailable in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

run_img2sixel -L librsvg! "${TOP_SRCDIR}/tests/data/inputs/formats/rgba.png" \
    >/dev/null && {
    echo "not ok" 1 - "forced librsvg loader accepted non-SVG signature"
    exit 0
}

echo "ok" 1 - "forced librsvg loader rejects non-SVG signature"
exit 0
