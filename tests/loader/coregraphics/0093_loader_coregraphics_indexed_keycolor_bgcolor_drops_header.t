#!/bin/sh
# Verify coregraphics indexed+tRNS drops keycolor header with bgcolor.

set -eux

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics support is disabled in this build\n"
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
    -L coregraphics! \
    -B#ffffff \
    -d fs:scan=raster \
    "${input_png}")" || {
    echo "not ok 1 - coregraphics indexed+tRNS bgcolor render failed"
    exit 0
}

test "${output_text#*"${keycolor_header}"}" = "${output_text}" || {
    echo "not ok 1 - coregraphics indexed+tRNS bgcolor output kept keycolor DCS header"
    exit 0
}

echo "ok 1 - coregraphics indexed+tRNS bgcolor output drops keycolor DCS header"
exit 0
