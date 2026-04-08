#!/bin/sh
# TAP test: gdk-pixbuf2 --start-frame=0 matches default frame selection.

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

input_gif="${TOP_SRCDIR}/tests/data/inputs/small.gif"
default_text=''
frame0_text=''

default_text="$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L gdk-pixbuf2! -ldisable -S \
    "${input_gif}")" || {
    echo "not ok 1 - gdk-pixbuf2 default start-frame decode failed"
    exit 0
}

frame0_text="$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --start-frame=0 -L gdk-pixbuf2! -ldisable -S \
    "${input_gif}")" || {
    echo "not ok 1 - gdk-pixbuf2 --start-frame=0 decode failed"
    exit 0
}

test "${default_text}" = "${frame0_text}" || {
    echo "not ok 1 - gdk-pixbuf2 --start-frame=0 output mismatched default"
    exit 0
}

echo "ok 1 - gdk-pixbuf2 --start-frame=0 matches default"
exit 0
