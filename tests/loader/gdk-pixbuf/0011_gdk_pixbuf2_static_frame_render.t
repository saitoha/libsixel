#!/bin/sh
# TAP test: gdk-pixbuf2 static frame rendering with animation flags succeeds.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


test "${HAVE_GDK_PIXBUF2-}" = 1 || {
    printf "1..0 # SKIP gdk-pixbuf2 support is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L gdk-pixbuf2! -ldisable -dnone -g \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" >/dev/null || {
    echo "not ok" 1 - "gdk-pixbuf2 static frame rendering failed"
    exit 0
}

echo "ok" 1 - "gdk-pixbuf2 static frame rendering succeeded"
exit 0
