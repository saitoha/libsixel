#!/bin/sh
# TAP test: gdk-pixbuf2 negative out-of-range start frame returns an error.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


test "${HAVE_GDK_PIXBUF2-}" = 1 || {
    printf "1..0 # SKIP gdk-pixbuf2 support is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"
set -v

run_img2sixel --start-frame=-999 \
    -L gdk-pixbuf2! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" \
    >/dev/null && {
    echo "not ok" 1 "out-of-range negative start frame unexpectedly succeeded"
    exit 0
}

echo "ok" 1 "gdk-pixbuf2 negative out-of-range start frame is rejected"
exit 0
