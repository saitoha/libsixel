#!/bin/sh
# TAP test confirming --loaders gdk-pixbuf2! forces PNG decoding path.

# Enable strict mode with verbose tracing for diagnostics.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
test "${HAVE_GDK_PIXBUF2-}" = 1 || {
    printf "1..0 # SKIP gdk-pixbuf2 support is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/rgba.png"

run_img2sixel -L gdk-pixbuf2! "${input_png}" >/dev/null || {
    fail 1 "gdk-pixbuf2 forced PNG decoding failed"
    exit 0
}

pass 1 "gdk-pixbuf2 forced PNG decoding succeeds"
exit 0
