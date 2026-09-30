#!/bin/sh
# Verify sRGB8 export after CIELAB application and P3 wire preparation.
# Policy: docs/encoder/palette-construction/fixed-palettes.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}
echo "1..1"
set -v

expected='JASC-PAL
0100
16
0 0 0
128 0 0
0 128 0
128 128 0
0 0 128
128 0 128
0 128 128
192 192 192
128 128 128
255 0 0
0 255 0
255 255 0
0 0 255
255 0 255
0 255 255
255 255 255'
actual=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    -Lbuiltin:cms_engine=none! -bxterm16 -Wcielab -dnone -Udisplay-p3 \
    -Mpal:- -o/dev/null "${TOP_SRCDIR}/tests/data/inputs/snake_16.png") || {
    echo "not ok 1 - builtin P3 CIELAB palette export failed"
    exit 0
}
actual=$(printf '%s' "${actual}" | tr -d '\015')
test "${actual}" = "${expected}" || {
    echo "not ok 1 - builtin palette export lost source sRGB8 entries"
    exit 0
}
echo "ok 1 - builtin P3 CIELAB export retains sRGB8 values and order"
exit 0
