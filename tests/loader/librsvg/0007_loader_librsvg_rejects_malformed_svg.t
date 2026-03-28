#!/bin/sh
# TAP test confirming forced librsvg loader rejects malformed SVG input.

set -eux

test "${HAVE_LIBRSVG-}" = 1 || {
    printf "1..0 # SKIP librsvg loader is unavailable in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

svg_path="${TOP_SRCDIR}/tests/data/inputs/formats/librsvg-malformed.svg"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L librsvg! "${svg_path}" -o/dev/null || status="$?"

test "${status-0}" -ne 0 || {
    echo "not ok" 1 - "forced librsvg unexpectedly accepted malformed SVG"
    exit 0
}

echo "ok" 1 - "forced librsvg rejects malformed SVG"
exit 0
