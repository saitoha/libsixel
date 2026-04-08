#!/bin/sh
# TAP test: gdk-pixbuf2 positive last-frame index decodes successfully.

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

input_gif="${TOP_SRCDIR}/tests/data/inputs/snake_64.gif"
pos_last_text=''
neg_last_text=''

pos_last_text="$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --start-frame=1 -L gdk-pixbuf2! -ldisable -S \
    "${input_gif}")" || {
    echo "not ok 1 - gdk-pixbuf2 positive last-frame decode failed"
    exit 0
}

neg_last_text="$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --start-frame=-1 -L gdk-pixbuf2! -ldisable -S \
    "${input_gif}")" || {
    echo "not ok 1 - gdk-pixbuf2 negative last-frame decode failed"
    exit 0
}

test "${pos_last_text}" = "${neg_last_text}" || {
    echo "not ok 1 - gdk-pixbuf2 positive last-frame output mismatched"
    exit 0
}

echo "ok 1 - gdk-pixbuf2 positive last-frame index is accepted"
exit 0
