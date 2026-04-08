#!/bin/sh
# Verify gdk-pixbuf2 indexed+tRNS keeps keycolor header without bgcolor.

set -eux

test "${HAVE_GDK_PIXBUF2-}" = 1 || {
    printf "1..0 # SKIP gdk-pixbuf2 support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/pal8-trns-key0.png"
keycolor_header="$(printf '\033P0;1q')"
output_text=''

output_text="$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L gdk-pixbuf2! \
    -d fs -y raster \
    "${input_png}")" || {
    echo "not ok 1 - gdk-pixbuf2 indexed+tRNS render failed"
    exit 0
}

test "${output_text#*"${keycolor_header}"}" != "${output_text}" || {
    echo "not ok 1 - gdk-pixbuf2 indexed+tRNS output lost keycolor DCS header"
    exit 0
}

echo "ok 1 - gdk-pixbuf2 indexed+tRNS output keeps keycolor DCS header"
exit 0
